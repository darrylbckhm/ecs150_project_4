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

  TVMStatus VMMemoryPoolDeallocate(TVMMemoryPoolID memory, void *pointer)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    int found = 0;
    int found2 = 0;

    if(pointer == NULL)
    {

      MachineResumeSignals(&sigstate);

      return VM_STATUS_ERROR_INVALID_PARAMETER;

    }
    
    for (vector<MemoryPool *>::iterator itr = memoryPools.begin(); itr != memoryPools.end(); itr++)
    {
      if ((*itr)->id == memory)
      {
        found = 1;
        list<Block *> *blocks = &(*itr)->blocks;
        bool erase = false;
        auto itr3 = blocks->begin();
        for (auto itr2 = blocks->begin(); itr2 != blocks->end(); itr2++)
        {
          if ((*itr2)->start == pointer)
          {
            found2 = 1;
            Block *currentBlock = *itr2;
            Block *nextBlock = *next(itr2);
            Block *prevBlock = *prev(itr2);

            currentBlock->free = true;

            if (itr2 != prev(blocks->end()))
            {
              if (nextBlock->free == true)
              {
                //nextBlock->start = currentBlock->start;
                nextBlock->size = currentBlock->size + nextBlock->size;
                //blocks->erase(itr2);
              }
            }

            if (itr2 != blocks->begin())
            {
              if (prevBlock->free == true)
              {
                prevBlock->size = currentBlock->size + prevBlock->size;
                //blocks->erase(itr2);
                erase = true;
                itr3 = itr2;
              }
            }
            
            
          }
        }

        if (erase)
          blocks->erase(itr3);

      }
    }
     
    if(!found || !found2)
    {


       MachineResumeSignals(&sigstate);

       return VM_STATUS_ERROR_INVALID_PARAMETER;

    }

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMMemoryPoolAllocate(TVMMemoryPoolID memory, TVMMemorySize size, void **pointer)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    int found = 0;

    TVMMemorySize bytesleft = 0;

    VMMemoryPoolQuery(VM_MEMORY_POOL_ID_SYSTEM, &bytesleft);

    if(size > bytesleft)
    {
      
      MachineResumeSignals(&sigstate);

      return VM_STATUS_ERROR_INSUFFICIENT_RESOURCES;

    }

    if(size == 0 || pointer == NULL)
    {

      MachineResumeSignals(&sigstate);

      return VM_STATUS_ERROR_INVALID_PARAMETER;

    }

    TVMMemorySize trueSize;
    if ((size % 64) == 0)
    {
      trueSize = size;
    }
    else
    {
      trueSize = size + 64 - (size % 64);
    }

    //cout << endl;
    //cout << endl << "id: " << memory << endl;
    //cout << "size: " << size << endl;
    //cout << "trueSize: " << trueSize << endl;

    for (vector<MemoryPool *>::iterator itr = memoryPools.begin(); itr != memoryPools.end(); itr++)
    {
      if ((*itr)->id == memory)
      {
        found = 1;
        //cout << "found id: " << (*itr)->id << endl;
        list<Block *> *blocks = &(*itr)->blocks;
        //printBlocks(*itr);
        //for (list<Block *>::iterator itr2 = blocks.begin(); itr2 != blocks.end(); itr2++)
        for (auto itr2 = blocks->begin(); itr2 != blocks->end(); itr2++)
        {
          //cout << "size: " << (*itr2)->size << endl;
          //cout << "trueSize: " << trueSize << endl;
          //cout << "free: " << (*itr2)->free << endl;
          //cout << "freetrue?: " << ((*itr2)->free == true) << endl;
          if (((*itr2)->free == true) && ((*itr2)->size >= trueSize))
          {
            Block *free = (*itr2);
            Block *block = new Block();
            block->start = free->start;
            block->size = trueSize;
            block->free = false;
            free->start = (char *)free->start + trueSize;
            free->size = free->size - trueSize;

            blocks->insert(itr2, block);

            *pointer = block->start;

          }
        }

        auto itr2 = prev(blocks->end());
        if ((*itr2)->size == 0)
        {
          blocks->erase(itr2);
        }

        //printBlocks(*itr);
      }
    }

    if(!found)
    {

      MachineResumeSignals(&sigstate);

      return VM_STATUS_ERROR_INVALID_PARAMETER;

    } 

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMMemoryPoolQuery(TVMMemoryPoolID memory, TVMMemorySizeRef bytesleft)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    if(bytesleft == NULL)
    {

      MachineResumeSignals(&sigstate);

      return VM_STATUS_ERROR_INVALID_PARAMETER;

    }

    int found = 0;

    TVMMemorySize sizeLeft = 0;

    for (vector<MemoryPool *>::iterator itr = memoryPools.begin(); itr != memoryPools.end(); itr++)
    {
      if ((*itr)->id == memory)
      {
        found = 1;
        //printBlocks(*itr);
        //cout << "found id: " << (*itr)->id << endl;
        list<Block *> *blocks = &(*itr)->blocks;
        //printBlocks(*itr);
        //for (list<Block *>::iterator itr2 = blocks.begin(); itr2 != blocks.end(); itr2++)
        for (auto itr2 = blocks->begin(); itr2 != blocks->end(); itr2++)
        {
          if ((*itr2)->free == true)
          {
            sizeLeft += (*itr2)->size;
          }
        }
      }
    }

    *bytesleft = sizeLeft;

    if(!found)
    {

      MachineResumeSignals(&sigstate);

      return VM_STATUS_ERROR_INVALID_PARAMETER;

    } 

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMMemoryPoolDelete(TVMMemoryPoolID memory)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    bool found = false;
    vector<MemoryPool *>::iterator itr2 = memoryPools.begin();
    for (vector<MemoryPool *>::iterator itr = memoryPools.begin(); itr != memoryPools.end(); itr++)
    {
      if ((*itr)->id == memory)
      {
        found = true;
        itr2 = itr;
      }
    }

    if (found)
    {
      for(auto itr3 = (*itr2)->blocks.begin(); itr3 != (*itr2)->blocks.end(); itr3++)
      {

        if((*itr3)->free == 0)
        {

           MachineResumeSignals(&sigstate);

           return VM_STATUS_ERROR_INVALID_STATE;

        }

      }
      memoryPools.erase(itr2);

    }

    if(!found)
    {

       MachineResumeSignals(&sigstate);

       return VM_STATUS_ERROR_INVALID_PARAMETER;

    }
    MachineResumeSignals(&sigstate);
     

    return VM_STATUS_SUCCESS;

  }

  TVMStatus VMMemoryPoolCreate(void *base, TVMMemorySize size, TVMMemoryPoolIDRef memory)
  {
    TMachineSignalState sigstate;
    MachineSuspendSignals(&sigstate);

    if(base == NULL || size == 0 || memory == NULL)
    {
      
      MachineResumeSignals(&sigstate);

      return VM_STATUS_ERROR_INVALID_PARAMETER;

    }

    MemoryPool *memPool = new MemoryPool;
    memPool->id = memoryPools.size();
    memPool->size = size;
    memPool->mem = base;

    Block *block = new Block();
    block->start = base;
    block->size = size;
    block->free = true;
    memPool->blocks.push_back(block);
    memoryPools.push_back(memPool);

    *memory = memPool->id;

    //cout << endl;
    //cout << "memPool id: " << memPool->id << endl;
    //cout << "memory id: " <<  *memory << endl;

    MachineResumeSignals(&sigstate);

    return VM_STATUS_SUCCESS;

  }


#ifdef __cplusplus
}
#endif
