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
#include <string>
#include <algorithm>
#include "VirtualMachine.h"
#include "Machine.h"
#include "VMCustom.h"
#include <sstream>


#ifdef __cplusplus
extern "C" {
#endif

  using namespace std;

  BPB *bpb = new BPB();
  vector<Cluster *> clusters;
  FAT *fatImg;

  string intToHex(int a)
  {
    stringstream stream;
    stream << std::hex << a;
    string hexStr(stream.str());

    hexStr = string(4 - hexStr.length(), '0').append(hexStr);

    return hexStr;

  }


  TVMStatus VMDirectoryOpen(const char *dirname, int *dirdescriptor)
  {

    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    for(int i = 0; i < bpb->RootEntry; i++)
    {

      SVMDirectoryEntry dirent;

      *dirdescriptor = fatImg->fd;

      VMDirectoryRead(*dirdescriptor, &dirent);
    }

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMDirectoryClose(int dirdescriptor)
  {

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMDirectoryRead(int dirdescriptor, SVMDirectoryEntryRef dirent)
  {

    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    uint8_t data[512];
    curThread->fileCallFlag = 0;

    MachineFileSeek(fatImg->fd, 512 * bpb->FirstRootSec, 0, fileCallback, curThread);
    curThread->state = VM_THREAD_STATE_WAITING;
    Scheduler(false);

    int size = (int)(bpb->RootDirSecs * (int)(bpb->BytsPerSec));
    int length = MAX_LENGTH;

    while(size > 0)
    {

      if(size < MAX_LENGTH)
        length = size;

      curThread->fileCallFlag = 0;
      MachineFileRead(fatImg->fd, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), length, fileCallback, curThread);
      curThread->state = VM_THREAD_STATE_WAITING;

      Scheduler(false);

      memcpy(data, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), 512);
      size -= MAX_LENGTH;

  }
    char dirname[13];
    memcpy(dirname, data + 0, 11);
    dirname[11] = '\0';

    char dcreate[2];
    memcpy(dcreate, data + 16, 2);

    cout << "dirname: " << dirname<< endl;

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMDirectoryRewind(int dirdescriptor)
  {

    return VM_STATUS_SUCCESS;


  }

  TVMStatus VMDirectoryCurrent(char *abspath)
  {

    return VM_STATUS_SUCCESS;


  }

  TVMStatus VMDirectoryChange(const char *path)
  {

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMDirectoryCreate(const char *dirname)
  {

    return VM_STATUS_SUCCESS;


  }

  TVMStatus VMDirectoryUnlink(const char *path)
  {

    return VM_STATUS_SUCCESS;

  }

  void extractFatImage(FAT *fat)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    fatImg = fat;

    // extract BPB

    int fd = fat->fd;

    // declare variables
    uint8_t rawBPB[512];
    int length = 512;

    // read in raw BPB 
    curThread->fileCallFlag = 0;
    MachineFileRead(fd, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), length, fileCallback, curThread);
    curThread->state = VM_THREAD_STATE_WAITING;

    MachineResumeSignals(&sigstate);
    Scheduler(false);
    MachineSuspendSignals(&sigstate);

    memcpy(rawBPB, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), length);

    // grab bytes and store in BPB class
    char name[10];
    memcpy(name, rawBPB + 2, 9);
    name[9] = '\0';

    bpb->OEMName = string(name);
    bpb->BytsPerSec =  *(uint16_t *)(rawBPB + 11);
    bpb->SecPerClus = *(uint8_t *)(rawBPB + 13);
    bpb->ReservedSecs = *(uint16_t *)(rawBPB + 14);
    bpb->fatCount = *(uint8_t *)(rawBPB + 16);
    bpb->RootEntry = *(uint16_t *)(rawBPB + 17);
    bpb->TotSec16 = *(uint16_t *)(rawBPB + 19);
    bpb->Media = *(uint8_t *)(rawBPB + 21);
    bpb->FatSize16 = *(uint16_t *)(rawBPB + 22);
    bpb->SecPerTrk = *(uint16_t *)(rawBPB + 24);
    bpb->NumHeads = *(uint16_t *)(rawBPB + 26);
    bpb->HiddSec = *(uint64_t *)(rawBPB + 28);
    bpb->TotSec32 = *(uint64_t *)(rawBPB + 32);
    bpb->DrvNumber = *(uint8_t *)(rawBPB + 36);
    bpb->reserved1 = *(uint8_t *)(rawBPB + 37);
    bpb->BootSig = *(uint8_t *)(rawBPB + 38);
    bpb->RootDirSecs = ((bpb->RootEntry * 32) + (bpb->BytsPerSec - 1)) / bpb->BytsPerSec;
    bpb->FirstRootSec = bpb->ReservedSecs + (bpb->fatCount * bpb->FatSize16);
    bpb->FirstDataSec = bpb->ReservedSecs + (bpb->fatCount * bpb->FatSize16) + bpb->RootDirSecs;

    int totSec = bpb->TotSec32;
    if (bpb->TotSec16 != 0)
      totSec = bpb->TotSec16;

    int datasec = totSec - (bpb->ReservedSecs + (bpb->fatCount * bpb->FatSize16) + bpb->RootDirSecs);
    bpb->ClusterCount = datasec / bpb->SecPerClus;

    char label[12];
    memcpy(label, rawBPB + 43, 11);
    label[11] = '\0';
    bpb->VolLab = string(label);

    char type[9];
    memcpy(type, rawBPB + 54, 8);
    type[8] = '\0';
    string s = "hello";
    bpb->FileSystemType = string(type);

    fat->bpb = bpb;

    for (int k = 1; k < bpb->FatSize16 + 1; k++)
    {
      // extract FAT Sector
      uint8_t data[512];
      curThread->fileCallFlag = 0;

      MachineFileSeek(fd, 512*k, 0, fileCallback, curThread);
      curThread->state = VM_THREAD_STATE_WAITING;
      Scheduler(false);

      curThread->fileCallFlag = 0;
      MachineFileRead(fd, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), length, fileCallback, curThread);
      curThread->state = VM_THREAD_STATE_WAITING;

      Scheduler(false);

      memcpy(data, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), length);

      for (int i = 1; i < 513; i++)
      {
        uint16_t a = *(uint16_t *)(data + ((2*i) - 2));
        string s = intToHex(a);

        Cluster *c = new Cluster();
        c->hex = string(s);

        clusters.push_back(c);
      }
    }

    //cout << "size: " << clusters.size() << endl;

    uint8_t data[512];
    curThread->fileCallFlag = 0;
    
    MachineFileSeek(fd, 512 * bpb->FirstRootSec, 0, fileCallback, curThread);
    curThread->state = VM_THREAD_STATE_WAITING;
    Scheduler(false);

    curThread->fileCallFlag = 0;
    MachineFileRead(fd, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), length, fileCallback, curThread);
    curThread->state = VM_THREAD_STATE_WAITING;

    Scheduler(false);

    memcpy(data, (char*)sharedmem + (curThread->threadID * MAX_LENGTH), length);

    MachineSuspendSignals(&sigstate);

    int offset = 0;
    bool exit = false;
    while (!exit)
    {
      if ((*(data + offset) - '0') == -16)
      {
        break;
      }

      if (*(data + offset + 11) == 0xF)
      {
        offset += 32;
        continue;
      }

      Directory *dir = new Directory();

      char sfn[12];
      memcpy(sfn, data + offset, 11);
      sfn[11] = '\0';

      dir->sfn = string(sfn);

      directories.push_back(dir);

      offset += 32;

    }

    /*for (auto itr = clusters.begin(); itr != clusters.end(); itr++)
      {
      cout << "data: ";
      for (int j = 0; j < 8; j++)
      {
      cout << (*itr)->hex << " ";
      itr++;
      }
      itr--;
      cout << endl;
      }*/

    MachineResumeSignals(&sigstate);

  }

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
