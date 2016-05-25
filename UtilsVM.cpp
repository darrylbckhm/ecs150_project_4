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

  void printBPB(BPB *bpb)
  {
    cout << "OEM Name            : " << bpb->OEMName << endl;
    cout << "Bytes per Sector    : " << bpb->BytsPerSec << endl;
    cout << "Sectors Per Cluster : " << bpb->SecPerClus << endl;
    cout << "Reserved Sectors    : " << bpb->ReservedSecs << endl;
    cout << "Fat Count           : " << bpb->fatCount << endl;
    cout << "Root Entry          : " << bpb->RootEntry << endl;
    cout << "Sector Count 16     : " << bpb->TotSec16 << endl;
    cout << "Media               : " << bpb->Media << endl;
    cout << "FAT Size 16         : " << bpb->FatSize16 << endl;
    cout << "Sectors Per Track   : " << bpb->SecPerTrk << endl;
    cout << "Head Count          : " << bpb->NumHeads << endl;
    cout << "Hidden Sector Count : " << bpb->HiddSec << endl;
    cout << "Sector Count 32     : " << bpb->TotSec32 << endl;
    cout << "Drive Number        : " << bpb->DrvNumber << endl;
    cout << "Boot Signature      : " << bpb->BootSig << endl;
    cout << "Volume ID           : " << bpb->VolID << endl;
    cout << "Volume Label        : " << bpb->VolLab << endl;
    cout << "File System Type    : " << bpb->FileSystemType << endl;
    cout << "Root Dir Sectors    : " << bpb->RootDirSecs << endl;
    cout << "First Root Sector   : " << bpb->FirstRootSec << endl;
    cout << "First Data Sector   : " << bpb->FirstDatSec << endl;
    cout << "Cluster Count       : " << bpb->ClusterCount << endl;
  }

  void printBlocks(MemoryPool *mempool)
  {
    for (vector<MemoryPool *>::iterator itr = memoryPools.begin(); itr != memoryPools.end(); itr++)
    {
      if (mempool != NULL)
      {
        if ((*itr)->id == mempool->id)
        {
          list<Block *> blocks = (*itr)->blocks;
          cout << endl;
          cout << "MemPool id: " << (*itr)->id << endl;
          for (list<Block *>::iterator itr2 = blocks.begin(); itr2 != blocks.end(); itr2++)
          {
            cout << "start: " << (*itr2)->start << ", ";
            cout << "size: " << (*itr2)->size << ", ";
            cout << "free: " << (*itr2)->free << endl;
          }
          cout << endl;
        }
      }

    }
  }  

  void printThreadInfo()
  {
    cout << endl;
    cout << "size of threads vector: " << threads.size() << endl;
    cout << "size of highQueue: " << highQueue.size() << endl;
    cout << "size of normalQueue: " << normalQueue.size() << endl;
    cout << "size of lowQueue: " << lowQueue.size() << endl;
    cout << "currentThread: " << curThread->threadID << endl;

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      TCB *thread = *itr;
      cout << "threadID: " << thread->threadID << ", ";
      cout << "threadState: ";
      switch(thread->state) {
        case VM_THREAD_STATE_DEAD:  cout << "Dead";
                                    break;
        case VM_THREAD_STATE_RUNNING:  cout << "Running";
                                       break;
        case VM_THREAD_STATE_READY:  cout << "Ready";
                                     break;
        case VM_THREAD_STATE_WAITING:  cout << "Waiting";
                                       break;
        default:                    break;
      }
      cout << ", sleepStatus: " << thread->sleep;
      cout << ", addedToQueue: " << thread->addedToQueue;
      cout << endl;
    }

    for (vector<Mutex *>::iterator itr = mutexes.begin(); itr != mutexes.end(); itr++)
    {
      Mutex *mutex = *itr;
      cout << "mutexID: " << mutex->mutexID;
      cout << ", locked: " << mutex->locked;
      TCB *thread = mutex->owner;
      if (thread)
        cout << ", owner: " << thread->threadID;
      else
        cout << ", owner: no owner";
      cout << endl;
    }

    cout << endl;
  }

  TVMStatus VMTickMS(int *tickmsref)
  {

    if(tickmsref == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    *tickmsref = glbl_tickms;

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMTickCount(TVMTickRef tickref)
  {

    MachineEnableSignals();

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    if(tickref == NULL)
    {

      MachineResumeSignals(&sigstate);    
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    }

    *tickref = ticksElapsed; 

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }
#ifdef __cplusplus
}
#endif
