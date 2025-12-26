/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

#if FOS_KERNEL	
#include <kern/mem/kheap.h>
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/conc/kspinlock.h>
#endif


//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	if (ptrPageInfo < &pageBlockInfoArr[0] || ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE/PAGE_SIZE])
			panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va)
{
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0 || idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;
void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//Your code is here

	dynAllocStart = daStart;
	dynAllocEnd = daEnd;

	for (int i = 0; i < LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1 ; i++)
	{
		LIST_INIT(&freeBlockLists[i]);
	}

	LIST_INIT(&freePagesList);

	for (uint32 pageAddress = dynAllocStart; pageAddress < dynAllocEnd; pageAddress += PAGE_SIZE)
	{
		struct PageInfoElement* pageInfo = to_page_info(pageAddress);
		pageInfo->block_size = 0;
		pageInfo->num_of_free_blocks = 0;
		LIST_INSERT_TAIL(&freePagesList, pageInfo);
	}


	//Comment the following line
	//panic("initialize_dynamic_allocator() Not implemented yet");

}

//===========================
// [2] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here

    uint32 vnum = (uint32)va;

    if (vnum < dynAllocStart || vnum >= dynAllocEnd)
        return 0;

    uint32 pageIndex = (vnum - dynAllocStart) / PAGE_SIZE;

    return pageBlockInfoArr[pageIndex].block_size;


	//Comment the following line
	//panic("get_block_size() Not implemented yet");
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
	//Your code is here

	if (size == 0)
	{
		return NULL;
	}

	uint32 blockSize = DYN_ALLOC_MIN_BLOCK_SIZE;
	uint32 fblIndex = LOG2_MIN_SIZE;

	while (blockSize < size)
	{
		blockSize <<= 1;
		fblIndex++;
	}

	fblIndex -= LOG2_MIN_SIZE;

	struct BlockElement* block;
	struct PageInfoElement* pageInfo;
	uint32 pageStartAddress;

	if (LIST_SIZE(&freeBlockLists[fblIndex]) > 0)
	{
		block = LIST_FIRST(&freeBlockLists[fblIndex]);
		LIST_REMOVE(&freeBlockLists[fblIndex], block);

		pageInfo = to_page_info((uint32)block);
		pageInfo->num_of_free_blocks--;

		return (void*)block;
	}
	else if (LIST_SIZE(&freePagesList) > 0)
	{
		pageInfo = LIST_FIRST(&freePagesList);
		LIST_REMOVE(&freePagesList, pageInfo);

		pageInfo->block_size = blockSize;
		pageInfo->num_of_free_blocks = PAGE_SIZE / blockSize;

		pageStartAddress = to_page_va(pageInfo);

		int result = get_page((void*)pageStartAddress);
		if (result != 0)
		{
			LIST_INSERT_HEAD(&freePagesList, pageInfo);
			return NULL;
		}

		for (int i = 0; i < pageInfo->num_of_free_blocks; i++)
		{
			uint32 blockStartAddress = pageStartAddress + i * blockSize;
			LIST_INSERT_HEAD(&freeBlockLists[fblIndex], (struct BlockElement*)blockStartAddress);
		}

		block = LIST_FIRST(&freeBlockLists[fblIndex]);
		LIST_REMOVE(&freeBlockLists[fblIndex], block);

		pageInfo->num_of_free_blocks--;

		return (void*)block;
	}
	else
	{
		uint32 fblNextIndex = fblIndex + 1;

		for (uint32 i = fblNextIndex; i < LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1; i++)
		{
			if (LIST_SIZE(&freeBlockLists[i]) > 0)
			{
				block = LIST_FIRST(&freeBlockLists[i]);
				LIST_REMOVE(&freeBlockLists[i], block);

				pageInfo = to_page_info((uint32)block);
				pageInfo->num_of_free_blocks--;

				return (void*)block;
			}
		}
	}


#if FOS_KERNEL
	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
	if (holding_kspinlock(&frame_lock))
	{
		release_kspinlock(&frame_lock);
		struct Env* curenv = get_cpu_proc();
		curenv->env_status = ENV_BLOCKED;
		LIST_INSERT_TAIL(&alloc_wait_queue, curenv);
		sched();
		acquire_kspinlock(&frame_lock);
		return alloc_block(size);
	}
	else
		panic("alloc_block() called without frame_lock held in kernel mode!");
#else
	panic("alloc_block() There aren't free blks in lists");
#endif
	//Comment the following line
	//panic("alloc_block() Not implemented yet");

	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
}

//===========================
// [4] FREE BLOCK:
//===========================

uint32 log2(uint32 n)
{
    uint32 result = 0;
    while (n > 1)
    {
        n >>= 1;
        result++;
    }
    return result;
}


void free_block(void *va)
{
	//==================================================================================
		//DON'T CHANGE THESE LINES==========================================================
		//==================================================================================
		{
			assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
		}
		//==================================================================================
		//==================================================================================

		//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
		//Your code is here
		 uint32 number1 = (uint32)va;

		    uint32 pageIndex = (number1 - dynAllocStart) / PAGE_SIZE;
		    struct PageInfoElement* page1 = &pageBlockInfoArr[pageIndex];

		    assert(page1->block_size != 0);

		    uint32 size1 = page1->block_size;

		    uint32 Index1 = log2(size1) - LOG2_MIN_SIZE;

		    LIST_INSERT_HEAD(&freeBlockLists[Index1], (struct BlockElement*)va);

		    page1->num_of_free_blocks++;

		    if (page1->num_of_free_blocks == PAGE_SIZE / size1)
		    {
		        uint32 StarTPage = to_page_va(page1);

		        struct BlockElement* blk = LIST_FIRST(&freeBlockLists[Index1]);

		        while (blk != NULL)
		        {
		            struct BlockElement* next = LIST_NEXT(blk);

		            uint32 address = (uint32)blk;
		            uint32 blk_pageIndex = (address
		            		- dynAllocStart) / PAGE_SIZE;

		            if (blk_pageIndex == pageIndex)
		            {
		                LIST_REMOVE(&freeBlockLists[Index1], blk);
		            }

		            blk = next;
		        }
		        // ================================

		        return_page((void*)StarTPage);

		        page1->block_size = 0;
		        page1->num_of_free_blocks = 0;

		        LIST_INSERT_HEAD(&freePagesList, page1);
		    }

		    //panic("free_block() Not implemented yet");

#if FOS_KERNEL
			if (!LIST_EMPTY(&alloc_wait_queue))
			{
				struct Env* waiter = LIST_FIRST(&alloc_wait_queue);
				LIST_REMOVE(&alloc_wait_queue, waiter);
				sched_insert_ready(waiter);
			}
#endif
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	//panic("realloc_block() Not implemented yet");
	if (va == NULL) return alloc_block(new_size);
	if (new_size == 0) {
		free_block(va);
		return NULL;
	}

	if (new_size > DYN_ALLOC_MAX_BLOCK_SIZE) return NULL; 

	uint32 old_size = get_block_size(va);
	if (new_size <= old_size) return va; 

	// Relocate
#if FOS_KERNEL
	bool held = holding_kspinlock(&frame_lock);
	if (!held) acquire_kspinlock(&frame_lock);
#endif

	void *new_va = alloc_block(new_size);
	
	if (new_va != NULL) {
		memcpy(new_va, va, old_size);
		free_block(va);
	}

#if FOS_KERNEL
	if (!held) release_kspinlock(&frame_lock);
#endif

	return new_va;
}
