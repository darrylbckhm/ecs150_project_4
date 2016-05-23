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

  TVMStatus VMMutexCreate(TVMMutexIDRef mutexref)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    if(mutexref == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    Mutex *mutex = new Mutex;
    mutex->locked = 0;
    mutex->mutexID = mutexes.size();

    mutexes.push_back(mutex);

    *mutexref = mutex->mutexID;

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);
    
    for (vector<Mutex *>::iterator itr = mutexes.begin(); itr != mutexes.end(); itr++)
    {
      if ((*itr)->mutexID == mutex)
      {
        if ((*itr)->locked == 0)
        {
          (*itr)->locked = 1;
          (*itr)->owner = curThread;
        }
        else
        {

          if(timeout == VM_TIMEOUT_IMMEDIATE)
          {

            Scheduler(false);
            MachineResumeSignals(&sigstate);
            return VM_STATUS_FAILURE;

          }


          if(timeout > 0)
          {

            curThread->state = VM_THREAD_STATE_WAITING;
            curThread->sleep = 1;
            curThread->sleepCount = timeout;
            Scheduler(false);
            if((*itr)->locked == 1)
            {
              MachineResumeSignals(&sigstate);
              return VM_STATUS_FAILURE;
            }

          }
          curThread->state = VM_THREAD_STATE_WAITING;

          if(curThread->priority == VM_THREAD_PRIORITY_HIGH)
          {

            (*itr)->highWaitingQueue.push(curThread);

          }

          else if(curThread->priority == VM_THREAD_PRIORITY_NORMAL)
          {

            (*itr)->normalWaitingQueue.push(curThread);

          }

          else if(curThread->priority == VM_THREAD_PRIORITY_LOW)
          {

            (*itr)->lowWaitingQueue.push(curThread);

          }

          MachineResumeSignals(&sigstate);
          Scheduler(false);

        }
      }
      else
      {

        return VM_STATUS_ERROR_INVALID_ID;

      }
    }

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMMutexRelease(TVMMutexID mutex)
  {

    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    int found = 0;

    for (vector<Mutex *>::iterator itr = mutexes.begin(); itr != mutexes.end(); itr++)
    {
      if ((*itr)->mutexID == mutex)
      {

        if((*itr)->owner != curThread)
        {

          MachineResumeSignals(&sigstate);
          return VM_STATUS_ERROR_INVALID_STATE;

        }

        found = 1;

        (*itr)->locked = 0;
        TCB *newOwner = NULL;
        if ((*itr)->highWaitingQueue.size() > 0)
        {
          newOwner = (*itr)->highWaitingQueue.front();
          (*itr)->highWaitingQueue.pop();
        }
        else if ((*itr)->normalWaitingQueue.size() > 0)
        {
          newOwner = (*itr)->normalWaitingQueue.front();
          (*itr)->normalWaitingQueue.pop();
        }
        else if ((*itr)->lowWaitingQueue.size() > 0)
        {
          newOwner = (*itr)->lowWaitingQueue.front();
          (*itr)->lowWaitingQueue.pop();
        }

        if (newOwner != NULL)
        {
          (*itr)->locked = 1;
          (*itr)->owner = newOwner;
          newOwner->state = VM_THREAD_STATE_READY;
          MachineResumeSignals(&sigstate);
          Scheduler(false);
        }
      }

      if(!found) 
      {

        MachineResumeSignals(&sigstate);
        return VM_STATUS_ERROR_INVALID_ID;

      }

    }

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref)
  {

    if(ownerref == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    int found = 0;
    
    for(vector<Mutex *>::iterator itr = mutexes.begin(); itr != mutexes.end(); itr++)
    {

      if((*itr)->mutexID == mutex && (*itr)->locked == 1)
      {

        found = 1;
        *ownerref = ((*itr)->owner)->threadID;

      }

      if((*itr)->mutexID == mutex && (*itr)->locked == 0)
      {

        found = 1;
        *ownerref = VM_THREAD_ID_INVALID;

      }

    }

    if(!found)
      return VM_STATUS_ERROR_INVALID_ID;

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMMutexDelete(TVMMutexID mutex)
  {

    int found = 0;

    for(vector<Mutex *>::iterator itr = mutexes.begin(); itr != mutexes.end(); itr++)
    {

      if(mutex == (*itr)->mutexID)
      {

        found = 1;

        if((*itr)->locked == 1)
        {

          return VM_STATUS_ERROR_INVALID_STATE;

        }

        mutexes.erase(itr);
	break;

      }

    }

    if(!found)
      return VM_STATUS_ERROR_INVALID_ID;

    return VM_STATUS_SUCCESS;

  }




#ifdef __cplusplus
}
#endif
