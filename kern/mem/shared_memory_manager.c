#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_kspinlock(&AllShares.shareslock, "shares lock");
	//init_sleeplock(&AllShares.sharessleeplock, "shares sleep lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//=========================
// [2] Find Share Object:
//=========================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* find_share(int32 ownerID, char* name)
{
#if USE_KHEAP
	struct Share * ret = NULL;
	bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld)
	{
		acquire_kspinlock(&(AllShares.shareslock));
	}
	{
		struct Share * shr ;
		LIST_FOREACH(shr, &(AllShares.shares_list))
		{
//			cprintf("shared var name = %s compared with %s\n", name, shr->name);
//			cprintf("shared var id = %d compared with %d\n",ownerID , shr->ownerID);
			if(shr->ownerID == ownerID && strcmp(name, shr->name)==0)
			{
				//cprintf("%s found\n", name);
				ret = shr;
				break;
			}
		}
	}
	if (!wasHeld)
	{
		release_kspinlock(&(AllShares.shareslock));
	}
	return ret;
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [3] Get Size of Share Object:
//==============================
int size_of_shared_object(int32 ownerID, char* shareName)
{
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = find_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}
//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=====================================
// [1] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* alloc_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
   	#if USE_KHEAP
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #1 alloc_share
	//Your code is here
	//Comment the following line
	//panic("alloc_share() is not implemented yet...!!");
	uint32 SI=sizeof(struct Share);
	struct Share* SHred_OBJ =(struct Share*)kmalloc(SI);
	//validation
	if(SHred_OBJ==NULL)
		return NULL;
    //initialization
	SHred_OBJ->references=1;
	int NAMES=sizeof(SHred_OBJ->name);
//	for(int i=0; i<NAMES;i++) {
//
//		SHred_OBJ->name[i]=shareName[i];
//	}
	strcpy(SHred_OBJ->name, shareName);

	SHred_OBJ->ownerID= ownerID;
    SHred_OBJ->ID=((uint32)SHred_OBJ)&  0x7FFFFFFF;//shift to make the most significant 0
    //SHred_OBJ->ID=((uint32)virtual_address)& ~(1 << 31);//shift to make the most significant 0
    SHred_OBJ->size = size;
	SHred_OBJ->isWritable=isWritable;
   	uint32 numOFp=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;

   	uint32 the_ss=numOFp*sizeof(struct FrameInfo*);

   	SHred_OBJ->framesStorage = (struct FrameInfo**)kmalloc(the_ss);
   	for(int i=0; i<0; i++){
   		cprintf("will not run");
   	}
   	//undo
   if(SHred_OBJ->framesStorage ==NULL){
	   kfree(SHred_OBJ);
	   return NULL;
   }
   //pointers to zero
	for(uint32 i=0 ; i<numOFp ; i++){

		SHred_OBJ->framesStorage[i]=NULL;

	}

	return SHred_OBJ;
    #else
	panic("not handled when KERN HEAP is disabled");
	#endif



}


//=========================
// [4] Create Share Object:
//=========================
int create_shared_object(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #3 create_shared_object
	//Your code is here
	//Comment the following line
	//panic("create_shared_object() is not implemented yet...!!");
	
#if USE_KHEAP
	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should create the shared object at the given virtual address with the given size
	// and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_EXISTS if the shared object already exists
	//	c) E_NO_SHARE if failed to create a shared object
//	struct Share * shr ;
//			LIST_FOREACH(shr, &(AllShares.shares_list)){
//				cprintf("shrf %d\n", shr->ownerID);
//			}

	//if the shared object already exists
	// cprintf("before if the id %d ",ownerID);
	 struct Share* thefl =find_share(ownerID,shareName);
	if(thefl != NULL){
		cprintf("the if ");
		return E_SHARED_MEM_EXISTS;
	}
	// cprintf("after if ");
	struct Share* SHred_OBJ = alloc_share(ownerID,shareName,size,isWritable);
	//if failed to create a shared object

	if (SHred_OBJ==NULL)return E_NO_SHARE;

	uint32 numOFp=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
	  for(uint32 i=0 ; i<numOFp ; i++){
		  struct FrameInfo* ELMAKAN = NULL;
		  allocate_frame(&ELMAKAN);
			for(int i=10;i<5; i++){
				cprintf("will not get into");
			}
		  	 if( ELMAKAN==NULL)	return E_NO_SHARE;

         //    if(isWritable)
		  	  map_frame(myenv->env_page_directory,ELMAKAN,(uint32) virtual_address,
		  				                  PERM_PRESENT | PERM_USER | PERM_WRITEABLE);//perms
//             else
//            	 map_frame(myenv->env_page_directory,ELMAKAN,(uint32) virtual_address,
//            	 		  				                  PERM_PRESENT | PERM_USER );//perms
            // cprintf("create_shared_object: mapped frame %p -> va %x (i=%d)\n", ELMAKAN, (uint32)virtual_address, i);

	  		SHred_OBJ->framesStorage[i]=ELMAKAN;
	  		//round down va?, trace
	  		virtual_address+=PAGE_SIZE;

	  	}

//	 //locks
	  if (!holding_kspinlock(&AllShares.shareslock))
	  acquire_kspinlock(&AllShares.shareslock);
//	  cprintf("SHred_OBJ %d\n", SHred_OBJ->ID);
//	  cprintf("SHred_OBJ %d\n", SHred_OBJ->ownerID);
	    LIST_INSERT_TAIL(&AllShares.shares_list, SHred_OBJ);

	           // SHred_OBJ->name, SHred_OBJ->ownerID, SHred_OBJ->ID, SHred_OBJ->size);
	   if (holding_kspinlock(&AllShares.shareslock))
	     release_kspinlock(&AllShares.shareslock);
//	     LIST_FOREACH(shr, &(AllShares.shares_list)){
//	     				cprintf("shr2 %d\n", shr->ownerID);
//	     			}
       return SHred_OBJ->ID;
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}


//======================
// [5] Get Share Object:
//======================
int get_shared_object(int32 ownerID, char* shareName, void* virtual_address)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #5 get_shared_object
	//Your code is here
	//Comment the following line
	//panic("get_shared_object() is not implemented yet...!!");
#if USE_KHEAP

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// 	This function should share the required object in the heap of the current environment
	//	starting from the given virtual_address with the specified permissions of the object: read_only/writable
	// 	and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists
	struct Share* THE_OBCT = find_share(ownerID,shareName);
		//if the shared object exists
		if(THE_OBCT==NULL)return E_SHARED_MEM_NOT_EXISTS;
		uint32 numOFp=ROUNDUP(THE_OBCT->size,PAGE_SIZE)/PAGE_SIZE;
        while(0){
        	cprintf("000");
        	return PAGE_SIZE;
        }
		         for (uint32 i = 0; i < numOFp; ++i) {
				  struct FrameInfo* ELMAKANnew = THE_OBCT->framesStorage[i];
					 if( ELMAKANnew==NULL)	return E_NO_SHARE;

				  if((THE_OBCT->isWritable)==1)
				  	  map_frame(myenv->env_page_directory,ELMAKANnew, (uint32)virtual_address,
				  				                  PERM_PRESENT | PERM_USER | PERM_WRITEABLE);//perms
				  else
					  map_frame(myenv->env_page_directory,ELMAKANnew, (uint32)virtual_address,
					  				  				                  PERM_PRESENT | PERM_USER);
			  		//round down va?, trace
			  		virtual_address+=PAGE_SIZE;

			  	}
		         if(1){
			  if (!holding_kspinlock(&AllShares.shareslock))
	           acquire_kspinlock(&AllShares.shareslock);
		       THE_OBCT->references++;
		      if (holding_kspinlock(&AllShares.shareslock))
     	      release_kspinlock(&AllShares.shareslock);
		         }
		      return THE_OBCT->ID;

#else
	panic("not handled when KERN HEAP is disabled");
#endif



}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//
//=========================
// [1] Delete Share Object:
//=========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare_abdo)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - free_share
	#if USE_KHEAP

	if (ptrShare_abdo == NULL) return;

	bool abdoh = holding_kspinlock(&AllShares.shareslock);
	if (!abdoh)
		acquire_kspinlock(&AllShares.shareslock);
	
	LIST_REMOVE(&(AllShares.shares_list), ptrShare_abdo);

	if (!abdoh)
		release_kspinlock(&AllShares.shareslock);

	ptrShare_abdo->prev_next_info.le_next = NULL; 
	ptrShare_abdo->prev_next_info.le_prev = NULL;
	
	if (ptrShare_abdo->framesStorage != NULL)
		kfree(ptrShare_abdo->framesStorage);

	kfree(ptrShare_abdo);
	#else
	panic("not handled when KERN HEAP is disabled");
#endif
}


//=========================
// [2] Free Share Object:
//=========================
int delete_shared_object(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - delete_shared_object
	//Your code is here
	//Comment the following line
	//panic("delete_shared_object() is not implemented yet...!!");

#if USE_KHEAP
	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should free (delete) the shared object from the User Heapof the current environment
	// If this is the last shared env, then the "frames_store" should be cleared and the shared object should be deleted
	// RETURN:
	//	a) 0 if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	// Steps:
	//	1) Get the shared object from the "shares" array (use get_share_object_ID())
    //	2) Unmap it from the current environment "myenv"
	//	3) If one or more table becomes empty, remove it
	//	4) Update references
	//	5) If this is the last share, delete the share object (use free_share())
	//	6) Flush the cache "tlbflush()"
    

	//	1) Get the shared object from the "shares" array (use get_share_object_ID())
	struct Share* ptr_share_abdo = NULL;
	bool abdoh = holding_kspinlock(&AllShares.shareslock);
	
	if (!abdoh) 
		acquire_kspinlock(&AllShares.shareslock);

	struct Share * shr_abdo ;
	LIST_FOREACH(shr_abdo, &(AllShares.shares_list))
	{
		if(shr_abdo->ID == sharedObjectID)
		{
			ptr_share_abdo = shr_abdo;
			break;
		}
	}
	if (ptr_share_abdo == NULL)
	{
		if (!abdoh) 
			release_kspinlock(&AllShares.shareslock);
		return E_SHARED_MEM_NOT_EXISTS;
	}


	//	2) Unmap it from the current environment "myenv"
	uint32 va_abdo = (uint32)startVA;
	uint32 pages_abdo = ROUNDUP(ptr_share_abdo->size, PAGE_SIZE) / PAGE_SIZE;

	for (uint32 i = 0; i < pages_abdo; i++)
	{
		unmap_frame(myenv->env_page_directory, va_abdo + i * PAGE_SIZE);
	}

	//	3) If one or more table becomes empty, remove it 
	uint32 start = (uint32)startVA;
	uint32 end = start + ptr_share_abdo->size;
	uint32 pdx_start = PDX(start);
	uint32 pdx_end = PDX(end-1);
	for (uint32 i = pdx_start; i <= pdx_end; i++)
	{
		uint32 *ptr_page_table;
		int ret = get_page_table(myenv->env_page_directory, i << 22, &ptr_page_table);
		if (ret == TABLE_IN_MEMORY && ptr_page_table != NULL)
		{
			bool is_empty = 1;
			for (int j = 0; j < 1024; j++)
			{
				if (ptr_page_table[j] != 0)
				{
					is_empty = 0;
					break;
				}
			}
			if (is_empty)
			{
				kfree(ptr_page_table);
				myenv->env_page_directory[i] = 0;
			}
		}
	}

	//	4) Update references  
	if (!abdoh)
		acquire_kspinlock(&AllShares.shareslock);

	ptr_share_abdo->references--;

	//	5) If this is the last share, delete the share object (use free_share())
	if (ptr_share_abdo->references == 0)
	{
		free_share(ptr_share_abdo);
	}
	if (!abdoh) 
		release_kspinlock(&AllShares.shareslock);
	
	//	6) Flush the cache "tlbflush()"
	tlbflush();

	return 0;
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}
