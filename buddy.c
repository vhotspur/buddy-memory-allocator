#include "buddy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef long int Native;
typedef unsigned long UNative;
typedef unsigned char Byte;
typedef UNative Address;

static void * heap = NULL;
static size_t lookupTableOffset = 0;
static size_t heapSize = 0;


#define DUMP_BYTES_PER_LINE 32
#define DUMP_BYTES_IN_GROUP 8
#define UNIT 8

#define TRACE_FLOW(...) \
	do { \
		printf("[flow]: %s", __func__); \
		printf(__VA_ARGS__); \
		printf(";\n"); \
	} while (0)
#define TRACE_DUMP(...) \
	do { \
		printf("[dump]: "); \
		printf(__VA_ARGS__); \
		printf("\n"); \
	} while (0)

#define WRITE_ON_HEAP(byteOffset, type, value) \
	do { \
		type * tmpHeapPointer = (type *)((Byte *)heap + (byteOffset)); \
		*tmpHeapPointer = value; \
	} while (0)
#define READ_FROM_HEAP(byteOffset, type) \
	(type)*(((Byte *)heap + (byteOffset)))

#define GET_FIRST_BLOCK_ADDRESS(blockSize) \
	READ_FROM_HEAP(lookupTableOffset + (blockSize) * sizeof(Address), Address)
#define WRITE_FIRST_BLOCK_ADDRESS(blockSize, address) \
	WRITE_ON_HEAP(lookupTableOffset + (blockSize) * sizeof(Address), Address, (address))

#define GET_ADDRESS_OF_PREVIOUS_SIBLING(baseAddress) \
	READ_FROM_HEAP((baseAddress) + 2*sizeof(Address), Address)
#define GET_ADDRESS_OF_FOLLOWING_SIBLING(baseAddress) \
	READ_FROM_HEAP((baseAddress) + 3*sizeof(Address), Address)

#define SET_ADDRESS_OF_PREVIOUS_SIBLING(baseAddress, newAddress) \
	WRITE_ON_HEAP((baseAddress) + 2*sizeof(Address), Address, newAddress)
#define SET_ADDRESS_OF_FOLLOWING_SIBLING(baseAddress, newAddress) \
	WRITE_ON_HEAP((baseAddress) + 3*sizeof(Address), Address, newAddress)

void _buddyInitTable(size_t sizesCount);
size_t _getBlockSizeNeeded(const size_t amountNeeded);
Address _allocateBlock(const size_t blockSize);

/**
 * 
 */
void buddyInit(size_t totalSize) {
	assert(sizeof(void *) == sizeof(Address));
	heap = malloc(totalSize);
	if (heap == 0) {
		exit(1);
	}
	memset(heap, 170, totalSize);
	heapSize = totalSize;
	
	// count what will be the biggest block size
	// and how many bytes will remain
	size_t allocableSize = totalSize;
	
	// decrement for the pointer to the table start and
	// for the size of the table
	allocableSize -= 2 * sizeof(Address);
	
	// here we count in units
	size_t sizeCount = 1;
	size_t maxBlockSize = 1;
	
	while (allocableSize + sizeof(Address) > UNIT * maxBlockSize) {
		// reserve space for table entry
		allocableSize -= sizeof(Address);
		maxBlockSize *= 2;
		sizeCount++;
	}
	// hmm, we overshot
	maxBlockSize /= 2;
	sizeCount--;
	
	_buddyInitTable(sizeCount);
}

void _buddyInitTable(size_t sizesCount) {
	TRACE_FLOW("(%lu)", sizesCount);
	lookupTableOffset = UNIT * ((Address)1 << (sizesCount-1)) + sizeof(Address);
	// write where the table starts
	WRITE_ON_HEAP(0, Address, lookupTableOffset);
	// write how many items are there
	WRITE_ON_HEAP(lookupTableOffset, Address, sizesCount);
	// write the pointers to the first free block of given size
	for (size_t i = 1; i < sizesCount; i++) {
		WRITE_ON_HEAP(lookupTableOffset + sizeof(Address)*i, Address, 0);
	}
	WRITE_ON_HEAP(lookupTableOffset + sizeof(Address)*sizesCount, Address, sizeof(Address));
	// initialize the (only) block
	WRITE_ON_HEAP(sizeof(Address)    , Address, sizesCount);
	WRITE_ON_HEAP(sizeof(Address) * 2, Address, 0);
	WRITE_ON_HEAP(sizeof(Address) * 3, Address, 0);
	WRITE_ON_HEAP(sizeof(Address) * 4, Address, 0);
}

/**
 * 
 */
void buddyDestroy() {
	free(heap);
}

/** 
 * 
 */
void * buddyMalloc(size_t amount) {
	TRACE_FLOW("(%lu)", amount);
	size_t amountNeeded = amount + 4 * sizeof(Address);
	size_t blockSize = _getBlockSizeNeeded(amountNeeded);
	TRACE_DUMP("will allocate %lu size block", blockSize);
	
	Address result = _allocateBlock(blockSize);
	if (result == 0) {
		return NULL;
	}
	TRACE_DUMP("returning offset %lu", result);
	return heap + result + 4*sizeof(Address);
}

size_t _getBlockSizeNeeded(const size_t amountNeeded) {
	size_t biggerBlockRealSize = UNIT;
	size_t blockSize = 1;
	while (amountNeeded > biggerBlockRealSize) {
		biggerBlockRealSize *= 2;
		blockSize++;
	}
	return blockSize;
}

int _hasAvailableBlock(const size_t blockSize) {
	TRACE_FLOW("(%lu)", blockSize);
	return GET_FIRST_BLOCK_ADDRESS(blockSize) != 0;
}

void _removeBlockFromList(Address addr, size_t blockSize) {
	TRACE_FLOW("(%lu, %lu)", addr, blockSize);
	Address previousBlockAddress = GET_ADDRESS_OF_PREVIOUS_SIBLING(addr);
	Address nextBlockAddress = GET_ADDRESS_OF_FOLLOWING_SIBLING(addr);
	
	// extra handling for single block in the list
	if ((previousBlockAddress == 0) && (nextBlockAddress == 0)) {
		WRITE_FIRST_BLOCK_ADDRESS(blockSize, 0);
		return;
	}
	
	// also first in the list got special handling
	if (previousBlockAddress == 0) {
		WRITE_FIRST_BLOCK_ADDRESS(blockSize, nextBlockAddress);
		SET_ADDRESS_OF_PREVIOUS_SIBLING(nextBlockAddress, 0);
		return;
	}
	
	// also the last item deserves special handling
	if (nextBlockAddress == 0) {
		SET_ADDRESS_OF_FOLLOWING_SIBLING(previousBlockAddress, 0);
		return;
	}
	
	// now we know that we are somewhere in the middle
	// of the list
	SET_ADDRESS_OF_PREVIOUS_SIBLING(nextBlockAddress, previousBlockAddress);
	SET_ADDRESS_OF_FOLLOWING_SIBLING(previousBlockAddress, nextBlockAddress);
}

int _createBlock(const size_t blockSize) {
	TRACE_FLOW("(%lu)", blockSize);
	if (_hasAvailableBlock(blockSize)) {
		return 0;
	}
	if (blockSize > READ_FROM_HEAP(lookupTableOffset, Address)) {
		assert(0);
	}
	// all right, block of this size does not exist - we need to
	// split a bigger block
	int parentOkay = _createBlock(blockSize + 1);
	if (parentOkay != 0) {
		return parentOkay;
	}
	
	// =========
	// we can now safely assume that there is a block of
	// blockSize+1 size that we can split
	// get the first from the list
	Address parentBlockAddress = GET_FIRST_BLOCK_ADDRESS(blockSize + 1);
	// remove it from the list
	_removeBlockFromList(parentBlockAddress, blockSize + 1);
	// and split it
	// as we know that there is no other block available, we can write
	// it's address to look-up table
	WRITE_FIRST_BLOCK_ADDRESS(blockSize, parentBlockAddress);
	// let's find the center of the block (there we split)
	Address blockHalfAddress = parentBlockAddress + UNIT * ((Address)1 << (blockSize-1));
	Address leftBlockAddress = parentBlockAddress;
	Address rightBlockAddress = blockHalfAddress;
	TRACE_DUMP("left block starts at %lu, right at %lu (parent starts at %lu)", 
		leftBlockAddress, rightBlockAddress, parentBlockAddress);
	// set addresses and size on the left half...
	WRITE_ON_HEAP(leftBlockAddress, Address, blockSize);
	WRITE_ON_HEAP(leftBlockAddress + sizeof(Address), Address, 0);
	SET_ADDRESS_OF_PREVIOUS_SIBLING(leftBlockAddress, 0);
	SET_ADDRESS_OF_FOLLOWING_SIBLING(leftBlockAddress, rightBlockAddress);
	// ... as well as on the right one
	WRITE_ON_HEAP(rightBlockAddress, Address, blockSize);
	WRITE_ON_HEAP(rightBlockAddress + sizeof(Address), Address, 0);
	SET_ADDRESS_OF_PREVIOUS_SIBLING(rightBlockAddress, leftBlockAddress);
	SET_ADDRESS_OF_FOLLOWING_SIBLING(rightBlockAddress, 0);
	buddyDump();
	// and that's it!
	return 0;
}

Address _allocateBlock(const size_t blockSize) {
	TRACE_FLOW("(%lu)", blockSize);
	int createOkay = _createBlock(blockSize);
	if (createOkay != 0) {
		return 0;
	}
	// we will get the first one
	Address firstBlockAddress = GET_FIRST_BLOCK_ADDRESS(blockSize);
	// remove it from the list
	_removeBlockFromList(firstBlockAddress, blockSize + 1);
	// and return it
	return firstBlockAddress;
}

/** 
 * 
 */
void buddyFree(void * ptr) {
	(void)ptr;
}

/**
 * 
 */
void buddyDump() {
	size_t address = 0;
	Byte * ptr = (Byte *)heap;
	while (address < heapSize) {
		size_t lineOffset = 0;
		printf(" %5lu:  ", address);
		while (lineOffset < DUMP_BYTES_PER_LINE) {
			if (address + lineOffset >= heapSize) {
				break;
			}
			printf("%02X ", *ptr);
			ptr++;
			lineOffset++;
			if ((lineOffset % DUMP_BYTES_IN_GROUP) == 0) {
				printf("  ");
			}
		}
		printf("\n");
		address += DUMP_BYTES_PER_LINE;
	}
}
