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

using namespace std;

extern "C" {

  const TVMMemoryPoolID VM_MEMORY_POOL_ID_SYSTEM = 0;
  void *sharedmem;
  vector<MemoryPool *> memoryPools;
  volatile unsigned int ticksElapsed;
  volatile unsigned int glbl_tickms;
  vector<FAT *> images;
  vector<Directory *> directories;
  vector<TCB*> threads;
  vector<Mutex *> mutexes;
  queue<TCB*> highQueue;
  queue<TCB*> normalQueue;
  queue<TCB*> lowQueue;
  TCB *curThread;
  TVMThreadID currentThreadID;

  void Scheduler(bool activate)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    TCB *prevThread = curThread;
    currentThreadID = prevThread->threadID;

    for(vector<TCB*>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {

      if(((*itr)->state == VM_THREAD_STATE_READY) && ((*itr)->addedToQueue == 0))
      {

        if((*itr)->priority == VM_THREAD_PRIORITY_HIGH)
        {

          (*itr)->addedToQueue = 1;
          highQueue.push((*itr));

        }

        else if((*itr)->priority == VM_THREAD_PRIORITY_NORMAL)
        {

          (*itr)->addedToQueue = 1;
          normalQueue.push((*itr));

        }

        else if((*itr)->priority == VM_THREAD_PRIORITY_LOW)
        {

          (*itr)->addedToQueue = 1;
          lowQueue.push((*itr));

        }

      }

    }

    if(!highQueue.empty())
    {

      curThread = highQueue.front();
      highQueue.pop();

    }

    else if(!normalQueue.empty())
    {

      curThread = normalQueue.front();
      normalQueue.pop();

    }

    else if(!lowQueue.empty())
    {

      curThread = lowQueue.front();
      lowQueue.pop();

    }

    if (curThread->threadID == prevThread->threadID)
    {
      if ((threads.size() == 2) && (threads[0]->state == VM_THREAD_STATE_RUNNING))
      {
        curThread = threads[0];
        return;
      }
      curThread = threads[1];
    }

    curThread->addedToQueue = 0;

    if (activate && (curThread->priority <= prevThread->priority))
    {
      curThread = prevThread;
      MachineResumeSignals(&sigstate);
      return;
    }

    if ((curThread->priority < prevThread->priority) && (prevThread->state == VM_THREAD_STATE_RUNNING))
    {
      curThread = prevThread;
      MachineResumeSignals(&sigstate);
      return;
    }

    if (prevThread->threadID == curThread->threadID)
    {
      MachineResumeSignals(&sigstate);
      return;
    }

    if (prevThread->state == VM_THREAD_STATE_RUNNING)
      prevThread->state = VM_THREAD_STATE_READY;


    curThread->state = VM_THREAD_STATE_RUNNING; 
    MachineResumeSignals(&sigstate);
    if (MachineContextSave(&prevThread->mcntx) == 0)
    {
      MachineContextRestore(&curThread->mcntx);
    }
    //MachineContextSwitch(&prevThread->mcntx, &curThread->mcntx);

    MachineResumeSignals(&sigstate);

  }

  void AlarmCall(void *param)
  {

    TMachineSignalState sigstate;

    MachineSuspendSignals(&sigstate);

    ticksElapsed++;

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->state == VM_THREAD_STATE_WAITING)
      {
        if ((*itr)->sleep == 1)
        {
          (*itr)->sleepCount = (*itr)->sleepCount - 1;
          if ((*itr)->sleepCount == 0)
          {
            (*itr)->sleep = 0;
            (*itr)->state = VM_THREAD_STATE_READY;
          }
        }
      }
    }

    MachineResumeSignals(&sigstate);

    Scheduler(false);

  }

  TVMStatus VMStart(int tickms, TVMMemorySize heapsize, TVMMemorySize sharedsize, const char * mount, int argc, char *argv[])
  {
    //cout << "heapsize: " << heapsize << endl;
    //cout << "sharedsize: " << sharedsize << endl;
    // create system memory pool
    

    MemoryPool *systemMemoryPool = new MemoryPool;
    systemMemoryPool->id = VM_MEMORY_POOL_ID_SYSTEM;
    systemMemoryPool->mem = (char *)malloc(heapsize * sizeof(char));
    systemMemoryPool->size = heapsize;

    // create free block of memory
    Block *block = new Block();
    block->start = systemMemoryPool->mem;
    block->size = heapsize;
    block->free = true;
    systemMemoryPool->blocks.push_back(block);

    memoryPools.push_back(systemMemoryPool);
      
    glbl_tickms = tickms;

    string module_name(argv[0]);
    TVMMainEntry main_entry = VMLoadModule(module_name.c_str());

    if(main_entry != NULL)
    {
      // create shared memory pool
      MemoryPool *sharedMemoryPool = new MemoryPool;
      sharedMemoryPool->id = memoryPools.size();
      sharedMemoryPool->mem = (char *)MachineInitialize(sharedsize);
      sharedMemoryPool->size = sharedsize;

      // create free block of memory
      Block *block = new Block();
      block->start = sharedMemoryPool->mem;
      block->size = sharedsize;
      block->free = true;
      sharedMemoryPool->blocks.push_back(block);

      memoryPools.push_back(sharedMemoryPool);

      sharedmem = memoryPools[1]->mem;
      
      MachineRequestAlarm(tickms*1000, AlarmCall, NULL);
      MachineEnableSignals();   

      // create threads (main and idle)

      TVMThreadID VMThreadID;
      VMThreadCreate(skeleton, NULL, 0x100000, VM_THREAD_PRIORITY_NORMAL, &VMThreadID);
      threads[0]->state = VM_THREAD_STATE_RUNNING;
      curThread = threads[0];

      VMThreadCreate(idle, NULL, 0x100000, VM_THREAD_PRIORITY_IDLE, &VMThreadID);
      VMThreadActivate(VMThreadID);

      // mount FAT image
      MachineFileOpen(mount, O_RDWR, 0600, fileCallback, curThread);
      curThread->state = VM_THREAD_STATE_WAITING;
      Scheduler(false);
      int fatfd = curThread->fileCallData;

      // create FAT image
      FAT *image = new FAT();
      image->fd = fatfd;
      extractFatImage(image);
      
      // store image in global vector
      images.push_back(image);


      main_entry(argc, argv);

    }

     else
     {

       return VM_STATUS_FAILURE;

     }

    return VM_STATUS_SUCCESS;

  }

}
