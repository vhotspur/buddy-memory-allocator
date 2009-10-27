/**
 * @file
 * Implementation of buddy memory allocator.
 * 
 */
#ifndef _BUDDY_H_GUARD
#define _BUDDY_H_GUARD

#include <stddef.h>

/** Initializes the buddy allocator. */
void buddyInit(size_t totalSize);

/** Destroys the buddy allocator. */
void buddyDestroy();

/** Allocates memory. */
void * buddyMalloc(size_t amount);

/** Frees allocated memory. */
void buddyFree(void * ptr);

/** Memory dump. */
void buddyDump();

#endif
