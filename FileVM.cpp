#include <iostream>
#include <unistd.h>
#include <list>
#include <vector>
#include <queue>
#include <iterator>
#include <stdint.h>
#include <cstdlib>
#include <memory.h>
#include <fcntl.h>

#include "VirtualMachine.h"
#include "Machine.h"
#include "VMCustom.h"


#ifdef __cplusplus
extern "C" {
#endif

  using namespace std;

  void fileCallback(void *calldata, int result)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    curThread->fileCallFlag = 1;

    TCB* thread = (TCB*)calldata;

    if(result > 0)
      thread->fileCallData = result;

    thread->state = VM_THREAD_STATE_READY;

    MachineResumeSignals(&sigstate);

    Scheduler(false);    

  }

  TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    curThread->fileCallFlag = 0;

    if (newoffset != NULL)
      *newoffset = offset;

    else
      return VM_STATUS_FAILURE;

    MachineFileSeek(filedescriptor, offset, whence, fileCallback, curThread);

    curThread->state = VM_THREAD_STATE_WAITING;

    MachineResumeSignals(&sigstate);

    Scheduler(false);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMFileClose(int filedescriptor)
  {

    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);
    curThread->fileCallFlag = 0;

    MachineFileClose(filedescriptor, fileCallback, curThread);

    MachineResumeSignals(&sigstate);

    Scheduler(false);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    curThread->fileCallFlag = 0;
    if (filename == NULL || filedescriptor == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    MachineFileOpen(filename, flags, mode, fileCallback, curThread);

    curThread->state = VM_THREAD_STATE_WAITING;

    Scheduler(false);

    *filedescriptor = curThread->fileCallData; 

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS; 

  }

  TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
  { 

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    curThread->fileCallFlag = 0;
    if (length == NULL || data == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    int tmp = *length;
    int numWrites = 0;

    while(tmp > MAX_LENGTH)
    {

      memcpy((char*)sharedmem + (curThread->threadID * MAX_LENGTH), (char*)data + (MAX_LENGTH * numWrites), MAX_LENGTH);

      MachineFileWrite(filedescriptor, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), MAX_LENGTH, fileCallback, curThread);

      curThread->state = VM_THREAD_STATE_WAITING;

      Scheduler(false);

      tmp = tmp - MAX_LENGTH;

      numWrites++;

    }

    memcpy((char*)sharedmem + (curThread->threadID * MAX_LENGTH), (char*)data + (MAX_LENGTH * numWrites), (size_t)(tmp));

    MachineFileWrite(filedescriptor, (char *)sharedmem + (curThread->threadID * MAX_LENGTH), tmp, fileCallback, curThread);

    curThread->state = VM_THREAD_STATE_WAITING;

    Scheduler(false);

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);
    curThread->fileCallFlag = 0;

    if (length == NULL || data == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;


    int *tmp = length;

    while(*tmp > MAX_LENGTH)
    {
    
      MachineFileRead(filedescriptor, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), MAX_LENGTH, fileCallback, curThread);
      curThread->state = VM_THREAD_STATE_WAITING;

      Scheduler(false);

      if(curThread->fileCallData > 0)
        *length = curThread->fileCallData;

      memcpy((char*)data, ((char*)sharedmem + (curThread->threadID * MAX_LENGTH)), MAX_LENGTH);

      *tmp = *tmp - MAX_LENGTH;

      continue;

    }

    MachineFileRead(filedescriptor, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), *length, fileCallback, curThread);

    curThread->state = VM_THREAD_STATE_WAITING;

    Scheduler(false);

    if(curThread->fileCallData > 0)
      *length = curThread->fileCallData;

    memcpy((char*)data, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), (size_t)(*length));

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

#ifdef __cplusplus
}
#endif
