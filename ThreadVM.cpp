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

  void skeleton(void *param)
  {
    MachineEnableSignals();
    TCB *thread = (TCB *)param;

    thread->entry(thread->param);


    VMThreadTerminate(curThread->threadID);

  }

  void idle(void *param)
  {
    while(1)
    {

      ;

    }

  }

  TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
  {

    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    if(entry == NULL || tid == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    TVMThreadID id = threads.size();

    TCB *thread = new TCB;
    thread->memsize = memsize;
    thread->status = VM_STATUS_SUCCESS;
    thread->sleep = 0;
    thread->threadID = id;
    thread->mutexID = -1;
    thread->priority = prio;
    thread->state = VM_THREAD_STATE_DEAD;
    thread->entry =  entry;
    thread->param = param;
    VMMemoryPoolAllocate(0, thread->memsize, (void **)&(thread->stackAddr));
    MachineContextCreate(&(thread->mcntx), skeleton, thread, thread->stackAddr, thread->memsize);

    threads.push_back(thread);

    *tid = id;
    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMThreadDelete(TVMThreadID thread)
  {

    int found = 0;

    for(vector<TCB*>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {

      if(thread == (*itr)->threadID)
      {

        if((*itr)->state != VM_THREAD_STATE_DEAD)
        {

          return VM_STATUS_ERROR_INVALID_STATE;

        }

        found = 1;
        threads.erase(itr);
	break;

      }

    }

    if(!found)
    {

      return VM_STATUS_ERROR_INVALID_ID;

    }

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMThreadID(TVMThreadIDRef threadref)
  {

     if(threadref == NULL)
       return VM_STATUS_ERROR_INVALID_PARAMETER;
     else
       *threadref = curThread->threadID;

     return VM_STATUS_SUCCESS;

  }

  TVMStatus VMThreadActivate(TVMThreadID thread)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    int found = 0;

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->threadID == thread)
      {

        found = 1;

        (*itr)->state = VM_THREAD_STATE_READY;
      }
    }

    if(!found)
      return VM_STATUS_ERROR_INVALID_ID;

    Scheduler(true);
    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMThreadTerminate(TVMThreadID thread)
  {
    int found = 0;

    for(vector<TCB*>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if((*itr)->threadID == thread)
      {

        found = 1; 

        if((*itr)->state == VM_THREAD_STATE_DEAD)
        {

          return VM_STATUS_ERROR_INVALID_STATE;

        }

        else
        {

	  for(vector<Mutex *>::iterator itr2 = mutexes.begin(); itr2 != mutexes.end(); itr2++)
          {

             if((*itr2)->locked == 1)
             {
               
               if ((*itr2)->owner->threadID == thread)
               {
                 
                 (*itr2)->locked = 0;
                 
               }
             }
	  }

          (*itr)->state = VM_THREAD_STATE_DEAD;
          Scheduler(false);

        }

      }

    }
      if(!found) 
      {

        return VM_STATUS_ERROR_INVALID_ID;
        
      }

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
  {

    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    int found = 0;

    if(stateref == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {

      if ((*itr)->threadID == thread)
      {
        *stateref = (*itr)->state;
        found = 1;
      }

    }

    if(!found)
    {

      MachineResumeSignals(&sigstate);
      return VM_STATUS_ERROR_INVALID_ID;

    }

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMThreadSleep(TVMTick tick)
  {
    if(tick == VM_TIMEOUT_INFINITE)
    {

      return VM_STATUS_ERROR_INVALID_PARAMETER;

    }

    if(tick == VM_TIMEOUT_IMMEDIATE)
    {

      Scheduler(false);
      return VM_STATUS_SUCCESS;

    }

    for (vector<TCB *>::iterator itr = threads.begin(); itr != threads.end(); itr++)
    {
      if ((*itr)->state == VM_THREAD_STATE_RUNNING)
      {
        (*itr)->sleepCount = tick;
        (*itr)->state = VM_THREAD_STATE_WAITING;
        (*itr)->sleep = 1;
        Scheduler(false);
      }
    }

    return VM_STATUS_SUCCESS;

  }


#ifdef __cplusplus
}
#endif
