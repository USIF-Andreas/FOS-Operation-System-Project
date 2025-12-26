#include <inc/lib.h>

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;

		__firstTimeFlag = 0;
	}
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================

struct UHBlk
{uint32 va;
uint32 size;
uint32 is_free;
struct UHBlk* next;}* UHBlkL = NULL;
void* malloc(uint32 size)
{
   //==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
	//Your code is here
    // Small allocations -> block allocator (smaller than max block size)
    if (size <= DYN_ALLOC_MAX_BLOCK_SIZE)
    return alloc_block(size);
    // Page allocator: exact -> worst -> extend (break < max)
    uint32 sz = ROUNDUP(size, PAGE_SIZE); // approximate size to page size
    // EXACT FIT --------------------------------------------------------------------
    struct UHBlk *cur_abdo = UHBlkL;
    while (cur_abdo != NULL) {
     if (cur_abdo->is_free && cur_abdo->size == sz) {
       cur_abdo->is_free = 0;
       sys_allocate_user_mem(cur_abdo->va, sz);
       return (void*)cur_abdo->va;
       }
        cur_abdo = cur_abdo->next;
    }
    // Worst fit when find largest free block >= sz  ---------------------------------
    struct UHBlk *worst = NULL;
    uint32 worst_size = 0; // the biggst size we try to found
	cur_abdo = UHBlkL;
    while (cur_abdo != NULL) {
		//check if free and size >= sz (to be able to allocate in it)
		// then check if its size > worst_size to be the worst fit and update worst_size
        if (cur_abdo->is_free && cur_abdo->size >= sz && cur_abdo->size > worst_size) {
                worst_size = cur_abdo->size;
                worst = cur_abdo;
        }
        cur_abdo = cur_abdo->next;
    }
    if (worst != NULL) {
    // we found the worst free block
    //now we split it into two parts : allocated part & free part
        struct UHBlk *newblk = alloc_block(sizeof(struct UHBlk));
		if (newblk == NULL) return NULL;
        newblk->va = worst->va + sz;
        newblk->size = worst->size - sz;
        newblk->is_free = 1;
        newblk->next = worst->next;
		worst->size = sz;
        worst->next = newblk;
        worst->is_free = 0;
        sys_allocate_user_mem(worst->va, sz);
        return (void*)worst->va;
    }
    // (if the worst is smaller than the requested size) Extend break if possible (break < max) --------------------------
    if ((uint32)(USER_HEAP_MAX - uheapPageAllocBreak) >= sz) {
		//if we can extend the heap
		//create a new block in the list
		//allocate the required pages & return its address & update the break & list
        struct UHBlk *nwblk_abdo = alloc_block(sizeof(struct UHBlk));
		if (nwblk_abdo == NULL) return NULL;
        nwblk_abdo->va = uheapPageAllocBreak;
        nwblk_abdo->size = sz;
        nwblk_abdo->is_free = 0;
        nwblk_abdo->next = NULL;
        if (UHBlkL == NULL)
        UHBlkL = nwblk_abdo;
        else {
        struct UHBlk *ex = UHBlkL;
        while (ex->next != NULL) ex = ex->next;
        ex->next = nwblk_abdo;
        }
        uheapPageAllocBreak += sz;
        sys_allocate_user_mem(nwblk_abdo->va, sz);
        return (void*)nwblk_abdo->va;
    }
    // No fit & cannot extend  ------------------------------------------------------
	return NULL;
    //Comment the following line
	// panic("malloc() is not implemented yet...!!");
}
//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
    //TODO: [PROJECT'25.IM#2] USER HEAP - #3 free
	//Your code is here
	//abdelrahman_code
	uint32 vboda = (uint32)virtual_address;
	if (vboda >= USER_HEAP_START && vboda < uheapPageAllocStart)
	{
	 free_block(virtual_address);
	 return;
	}
	if (vboda >= uheapPageAllocStart && vboda < USER_HEAP_MAX)
	{
	struct UHBlk* curAbdo = UHBlkL;
	struct UHBlk* prvX = NULL;
	while (curAbdo != NULL) {
	 if (curAbdo->va == vboda)
	 {
	 curAbdo->is_free = 1;
	 sys_free_user_mem(curAbdo->va, curAbdo->size);
	 if (curAbdo->next != NULL && curAbdo->next->is_free) {
	  struct UHBlk* nxBlk = curAbdo->next;
	  curAbdo->size += nxBlk->size;
	  curAbdo->next = nxBlk->next;
	  free_block(nxBlk);
	  }
	  if (prvX != NULL && prvX->is_free)
	  {
	   prvX->size += curAbdo->size;
	   prvX->next = curAbdo->next;
	   free_block(curAbdo);
	   curAbdo = prvX;
	  }
	  if (curAbdo->next == NULL)
	  {
	  if (curAbdo->va + curAbdo->size == uheapPageAllocBreak){
		uheapPageAllocBreak = curAbdo->va;
		if (prvX != NULL)
		prvX->next = NULL;
		else
		UHBlkL = NULL;
		free_block(curAbdo);
		}
	  }
		return;
		}
		prvX = curAbdo;
		curAbdo = curAbdo->next;
		}
		panic("Free had invalid address");
    	}
	panic("Free had invalid address");
    //Comment the following line
	// panic("free() is not implemented yet...!!");
}
//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
	//Your code is here
	//Comment the following line
	//panic("smalloc() is not implemented yet...!!");

	 uint32 sz = ROUNDUP(size, PAGE_SIZE); // page-needed
	//int fffff= sys_size_of_shared_object(sys_getenvid(),sharedVarName);
	//if(fffff>=1 )return NULL;
	    // EXACT FIT
	    struct UHBlk *cur_abdo = UHBlkL;
	    while (cur_abdo != NULL) {
	        if (cur_abdo->is_free && cur_abdo->size == sz) {

	            //sys_allocate_user_mem(cur_abdo->va, sz);
	           cprintf("first 1");
		        int ret =sys_create_shared_object(sharedVarName, sz,  isWritable,(void*)cur_abdo->va );
		        cprintf("smalloc: sys_create_shared_object returned %d for va=%x name=%s\n", ret, cur_abdo->va, sharedVarName);

		        cprintf("first 2");
		        if(ret  ==E_SHARED_MEM_EXISTS){ cur_abdo->is_free=1;return NULL;}
		        cur_abdo->is_free = 0;
	            return (uint32*)cur_abdo->va;
	        }
	        cur_abdo = cur_abdo->next;
	    }
	    // Worst fit when find largest free block >= sz
	    struct UHBlk *worst = NULL;
	    uint32 worst_size = 0;
	    struct UHBlk *cur_boda = UHBlkL;
	    while (cur_boda != NULL) {
	        if (cur_boda->is_free && cur_boda->size >= sz) {
	            if (cur_boda->size > worst_size) {
	                worst_size = cur_boda->size;
	                worst = cur_boda;
	            }
	        }
	        cur_boda = cur_boda->next;
	    }
	    if (worst != NULL) {
	        if (worst->size == sz) {

	           // sys_allocate_user_mem(worst->va, sz);
	            cprintf("sec 1");
		      int ret =  sys_create_shared_object(sharedVarName, sz,  isWritable,(void*)worst->va );
		        cprintf("smalloc: sys_create_shared_object returned %d for va=%x name=%s\n", ret, worst->va, sharedVarName);

		      cprintf("sec 2");
		        if(ret ==E_SHARED_MEM_EXISTS){ worst->is_free = 1;return NULL;}

		        worst->is_free = 0;
	            return (uint32*)worst->va;
	        }
	        struct UHBlk *newblk = alloc_block(sizeof(struct UHBlk));
	        if (newblk == NULL) return NULL;
	        newblk->va = worst->va + sz;
	        newblk->size = worst->size - sz;
	        newblk->is_free = 1;
	        newblk->next = worst->next;
	        worst->size = sz;
	        worst->next = newblk;
	       // worst->is_free = 0;
	       // sys_allocate_user_mem(worst->va, sz);
	        cprintf("third 1");
	      int ret=  sys_create_shared_object(sharedVarName, sz,  isWritable,(void*)worst->va );
	      cprintf("smalloc: sys_create_shared_object returned %d for va=%x name=%s\n", ret, worst->va, sharedVarName);

	      cprintf("third 2");
	        if(ret ==E_SHARED_MEM_EXISTS) {
	        	worst->is_free = 1;
	        	return NULL;}
	        worst->is_free = 0;
	        return (uint32*)worst->va;
	    }
	    // Extend break if possible (break < max)
	    if ((uint32)(USER_HEAP_MAX - uheapPageAllocBreak) >= sz) {
	        struct UHBlk *nd_abdo = alloc_block(sizeof(struct UHBlk));
	        if (nd_abdo == NULL) return NULL;
	        nd_abdo->va = uheapPageAllocBreak;
	        nd_abdo->size = sz;
	       // nd_abdo->is_free = 0;
	        nd_abdo->next = NULL;
	        if (UHBlkL == NULL) {
	            UHBlkL = nd_abdo;
	        } else {
	            struct UHBlk *t = UHBlkL;
	            while (t->next != NULL) t = t->next;
	            t->next = nd_abdo;
	        }
	        uheapPageAllocBreak += sz;
	       // sys_allocate_user_mem(nd_abdo->va, sz);
	        cprintf("fourth 1 ");
	      int ret=  sys_create_shared_object(sharedVarName, sz,  isWritable,(void*)nd_abdo->va );
	      cprintf("smalloc: sys_create_shared_object returned %d for va=%x name=%s\n", ret, nd_abdo->va, sharedVarName);

	      cprintf("fourth 2 %d ",ret);
	      if(ret  ==E_SHARED_MEM_EXISTS) {
	    	  cprintf("the if in smalloc ");
	    	  nd_abdo->is_free=1;
	    	  return NULL;}
	      nd_abdo->is_free = 0;
	     // cprintf("nd_abdo->va %d ",nd_abdo->va);
	        return (uint32*)nd_abdo->va;

	    }
	    // No fit & cannot extend
	    cprintf("final ");
	    return NULL;


}

//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget
	//Your code is here
	//Comment the following line
	//panic("sget() is not implemented yet...!!");

	uint32 size=sys_size_of_shared_object(ownerEnvID,sharedVarName);
	//If not exists, return NULL????
    if(size ==E_SHARED_MEM_NOT_EXISTS)return NULL;
	 uint32 sz = ROUNDUP(size, PAGE_SIZE); // page-needed
		    // EXACT FIT
		    struct UHBlk *cur_abdo = UHBlkL;
		    while (cur_abdo != NULL) {
		        if (cur_abdo->is_free && cur_abdo->size == sz) {
		            cur_abdo->is_free = 0;
		            //sys_allocate_user_mem(cur_abdo->va, sz);
		            int ret =  sys_get_shared_object(ownerEnvID,sharedVarName,(void*)cur_abdo->va);
			        if(ret==E_SHARED_MEM_NOT_EXISTS) return NULL;
		            return (void*)cur_abdo->va;
		        }
		        cur_abdo = cur_abdo->next;
		    }
		    // Worst fit when find largest free block >= sz
		    struct UHBlk *worst = NULL;
		    uint32 worst_size = 0;
		    struct UHBlk *cur_boda = UHBlkL;
		    while (cur_boda != NULL) {
		        if (cur_boda->is_free && cur_boda->size >= sz) {
		            if (cur_boda->size > worst_size) {
		                worst_size = cur_boda->size;
		                worst = cur_boda;
		            }
		        }
		        cur_boda = cur_boda->next;
		    }
		    if (worst != NULL) {
		        if (worst->size == sz) {
		            worst->is_free = 0;
		           // sys_allocate_user_mem(worst->va, sz);
		            int ret =  sys_get_shared_object(ownerEnvID,sharedVarName,(void*)worst->va);
		          	if(ret==E_SHARED_MEM_NOT_EXISTS) {
		          		return NULL;
		          	}
		            return (void*)worst->va;
		        }
		        struct UHBlk *newblk = alloc_block(sizeof(struct UHBlk));
		        if (newblk == NULL) return NULL;
		        newblk->va = worst->va + sz;
		        newblk->size = worst->size - sz;
		        newblk->is_free = 1;
		        newblk->next = worst->next;
		        worst->size = sz;
		        worst->next = newblk;
		        worst->is_free = 0;
		       // sys_allocate_user_mem(worst->va, sz);
		        int ret =  sys_get_shared_object(ownerEnvID,sharedVarName,(void*)worst->va);
		        if(ret==E_SHARED_MEM_NOT_EXISTS) return NULL;
		        return (void*)worst->va;
		    }
		    // Extend break if possible (break < max)
		    if ((uint32)(USER_HEAP_MAX - uheapPageAllocBreak) >= sz) {
		        struct UHBlk *nd_abdo = alloc_block(sizeof(struct UHBlk));
		        if (nd_abdo == NULL) return NULL;
		        nd_abdo->va = uheapPageAllocBreak;
		        nd_abdo->size = sz;
		        nd_abdo->is_free = 0;
		        nd_abdo->next = NULL;
		        if (UHBlkL == NULL) {
		            UHBlkL = nd_abdo;
		        } else {
		            struct UHBlk *t = UHBlkL;
		            while (t->next != NULL) t = t->next;
		            t->next = nd_abdo;
		        }
		        uheapPageAllocBreak += sz;
		       // sys_allocate_user_mem(nd_abdo->va, sz);

		        int ret =  sys_get_shared_object(ownerEnvID,sharedVarName,(void*)nd_abdo->va);
		      	if(ret==E_SHARED_MEM_NOT_EXISTS) return NULL;
		        return (void*)nd_abdo->va;
		    }
		    // No fit & cannot extend
		    return NULL;

}



//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	//panic("realloc() is not implemented yet...!!");
	uint32 old_sz = 0;
	uint32 addr_abdo = (uint32)virtual_address;
	if (virtual_address == NULL)
	return malloc(new_size);
	if (new_size == 0)
	{
	free(virtual_address);
 	return NULL;
	}
	// small blocks
	if (addr_abdo >= USER_HEAP_START && addr_abdo < USER_HEAP_START + DYN_ALLOC_MAX_SIZE)
	old_sz = get_block_size(virtual_address);
	// page allocator blocks
	else if (addr_abdo >= uheapPageAllocStart && addr_abdo < USER_HEAP_MAX)
	{
	 struct UHBlk *cur_abdo = UHBlkL;
	  while (cur_abdo != NULL)
	  {
	  if (cur_abdo->va == addr_abdo)
	  {
	   old_sz = cur_abdo->size;
       break;
	  }
	  cur_abdo = cur_abdo->next;
	  }
	}
	else
	return NULL;
	if (old_sz == 0)
	return NULL;
	void *new_abdo = malloc(new_size);
	if (new_abdo == NULL)
	return NULL;
    uint32 copy_sz = old_sz;
    if (new_size < old_sz)
    copy_sz = new_size;
	sys_move_user_mem(addr_abdo, (uint32)new_abdo, copy_sz);
	free(virtual_address);
	return new_abdo;
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
