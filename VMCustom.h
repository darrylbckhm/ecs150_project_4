#ifndef VMCUSTOM_H
#define VMCUSTOM_H

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

#define VM_THREAD_PRIORITY_IDLE 0
#define MAX_LENGTH 512


#ifdef __cplusplus
extern "C" {
#endif

  using namespace std;

  class BPB
  {
    public:
      string OEMName;
      string VolID;
      string VolLab;
      string FileSystemType;

      int BytsPerSec;
      int SecPerClus;
      int ReservedSecs;
      int fatCount;
      int RootEntry;
      int TotSec16;
      int Media;
      int FatSize16;
      int SecPerTrk;
      int NumHeads;
      int HiddSec;
      int TotSec32;
      int DrvNumber;
      int reserved1;
      int BootSig;
      int RootDirSecs;
      int FirstRootSec;
      int FirstDataSec;
      int ClusterCount;

  };

    
  class FAT
  {
    public:
      int fd;
      BPB *bpb;
  };


  class Block
  {
    public:
      void *start;
      TVMMemorySize size;
      bool free;
  };

  class MemoryPool {
    public:
      TVMMemoryPoolID id;
      void *mem;
      TVMMemorySize size;
      list<Block *> blocks;

  };

  class TCB {

    public: 
      TVMMemorySize memsize;
      TVMStatus status;
      TVMTick tick;
      TVMThreadID threadID;
      TVMMutexID mutexID;
      TVMThreadPriority priority;
      TVMThreadState state;
      char *stackAddr;
      volatile unsigned int sleepCount;
      // not sleeping = 0; sleeping = 1;
      unsigned int sleep;
      unsigned int addedToQueue;
      unsigned int fileCallFlag;
      volatile unsigned int fileCallData;

      SMachineContext mcntx;

      TVMThreadEntry entry;
      void *param;

      void (*TVMMainEntry)(int, char*[]);
      void (*TVMThreadEntry)(void *);

      void AlarmCall(void *param);

      TMachineSignalStateRef sigstate;

  };

  class Mutex {
    public:
      TVMMutexID mutexID;
      TCB *owner;
      unsigned int locked;
      queue<TCB *> highWaitingQueue;
      queue<TCB *> normalWaitingQueue;
      queue<TCB *> lowWaitingQueue;
  };

  extern void *sharedmem;

  extern vector<MemoryPool *> memoryPools;
  //static volatile unsigned int system_memory_pool_size;

  // keep track of total ticks
  extern volatile unsigned int ticksElapsed;

  extern volatile unsigned int glbl_tickms;
 
  // vector of all images
  extern vector<FAT *> images;

  // vector of all threads
  extern vector<TCB*> threads;
  extern vector<Mutex *> mutexes;

  // queue of threads sorted by priority
  extern queue<TCB*> highQueue;
  extern queue<TCB*> normalQueue;
  extern queue<TCB*> lowQueue;

  extern TCB *curThread;
  extern TVMThreadID currentThreadID;

  // declaration of custom functions
  TVMMainEntry VMLoadModule(const char *module);
  void skeleton(void *param);
  void Scheduler(bool activate);
  void idle(void *param);
  void printThreadInfo();
  void printBPB(BPB *bpb);
  void fileCallback(void *calldata, int result);

  void printBlocks(MemoryPool *mempool);
  void printThreadInfo();
  TVMStatus VMTickMS(int *tickmsref);
  TVMStatus VMTickCount(TVMTickRef tickref);

  TVMStatus VMMutexRelease(TVMMutexID mutex);
  TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout);
  TVMStatus VMMutexCreate(TVMMutexIDRef mutexref);
  TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref);
  TVMStatus VMMutexDelete(TVMMutexID mutex);

  TVMStatus VMThreadDelete(TVMThreadID thread);
  TVMStatus VMThreadID(TVMThreadIDRef threadref);
  TVMStatus VMThreadActivate(TVMThreadID thread);
  TVMStatus VMThreadTerminate(TVMThreadID thread);
  TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref);
  TVMStatus VMThreadSleep(TVMTick tick);
  TVMStatus VMThreadActivate(TVMThreadID thread);
  TVMStatus VMThreadTerminate(TVMThreadID thread);

  void fileCallback(void *calldata, int result);
  TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset);
  TVMStatus VMFileClose(int filedescriptor);
  TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor);
  TVMStatus VMFileWrite(int filedescriptor, void *data, int *length);
  TVMStatus VMFileRead(int filedescriptor, void *data, int *length);

  void extractFatImage(FAT *fat);



#ifdef __cplusplus
}
#endif

#endif
