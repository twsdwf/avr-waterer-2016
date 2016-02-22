/*
 * memdebug.h
 *
 *  Created on: 15 Dec 2010
 *      Author: Andy Brown
 *
 *  Use without attribution is permitted provided that this
 *  header remains intact and that these terms and conditions
 *  are followed:
 *
 *  http://andybrown.me.uk/ws/terms-and-conditions
 */


// #ifdef DEBUG


#ifdef __cplusplus
extern "C" {
#endif


typedef struct __freelist {
  size_t sz;
  struct __freelist *nx;
} FREELIST;


#include <stddef.h>


extern size_t getMemoryUsed();
extern size_t getFreeMemory();
extern size_t getLargestAvailableMemoryBlock();
extern size_t getLargestBlockInFreeList();
extern int getNumberOfBlocksInFreeList();
extern size_t getFreeListSize();
extern size_t getLargestNonFreeListBlock();
// extern void dumpFreeMemoryBlocks();

#ifdef __cplusplus
}
#endif


// #endif // DEBUG
