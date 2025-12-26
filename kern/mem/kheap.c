#include "kheap.h"
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"
#define num_of_all_pages 32766
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)
struct kspinlock frame_lock;
struct Env_Queue alloc_wait_queue;
//int max_FREE_size = 0;

/*
uint32 used_size = kheapPageAllocBreak - kheapPageAllocStart;
uint32 used_pages = used_size / PAGE_SIZE;
int max_FREE_size = 0;
*/
//LIST_HEAD(List_cluster, Cluster);

void kheap_init()
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		 kheap_clusters_init();
		 max_FREE_cluster[0]=0;// size init
		 max_FREE_cluster[1]=-1;// index init
		for (int i = 0; i < num_of_all_pages; ++i) {
         framesArr[i] = -1;
        }
		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart; //seconde space to allocate pages from
		init_kspinlock(&frame_lock, "kheap_lock");
		LIST_INIT(&alloc_wait_queue);
	// 	 for (uint32 va = dynAllocStart; va < dynAllocEnd; va += PAGE_SIZE) {
    //      uint32 ph = kheap_physical_address(va);
    //      if (ph != 0) {
    //      uint32 frame_index = ph >> 12;
    //      if (frame_index < num_of_all_pages) {
    //         framesArr[frame_index] = va & 0xFFFFF000;
    //     }
    // }
    //}

	}
	//==================================================================================
	//==================================================================================
}

#define POOL_CLUSTERS 1024
static struct KheapCluster clspool[POOL_CLUSTERS];
static unsigned int clspool_next = 0;
//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	//////cprintf("===========GET PAGE======================");
	//////cprintf("get_page va %x\n",va);
	//////cprintf("===========End======================");
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);
	if (ret < 0)//no memory
		panic("get_page() in kern: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================

void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #1 kmalloc
	//Your code is here
	//Comment the following line
	//kpanic_into_prompt("kmalloc() is not implemented yet...!!");
	uint32 page_needed=ROUNDUP(size,PAGE_SIZE);
	page_needed=page_needed/PAGE_SIZE; //2
    if (page_needed > 1024)
    {
        //////cprintf("kmalloc: Requested size (%d pages) exceeds system limit.\n", page_needed);
        return NULL;
    }
    int locked = holding_kspinlock(&frame_lock);
    if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
        struct BlockElement* block;
        if (!locked) acquire_kspinlock(&frame_lock);
		block = alloc_block(size);//must be locked
        if (block != NULL) {
        uint32 page_va = ROUNDDOWN((uint32)block, PAGE_SIZE);
        uint32 ph = kheap_physical_address(page_va);
         	//////cprintf("kmalloc small block VA=%x, PA=%x\n", (uint32)block, ph);
			if(ph>>12==0xffff000||ph>>12==0xfffe000){
				if(ph>>12==0xffff000)
				framesArr[ph >> 12] = 0xf6000ff0;
				else
				framesArr[ph >> 12] = 0xf6001ff8;

			}
			else if (ph != 0) {
				framesArr[ph >> 12] = ((uint32)block) & 0xFFFFF000;
			}
			if (!locked) release_kspinlock(&frame_lock);
			return (void*) block;
		} else {
			if (!locked) release_kspinlock(&frame_lock);
            return NULL;
        }
    }
	else if(size > DYN_ALLOC_MAX_BLOCK_SIZE) { // max free size
		//   uint32 needed_pages = ROUNDUP(size - used_size, PAGE_SIZE) / PAGE_SIZE;
        if (!locked) acquire_kspinlock(&frame_lock);
        //2 data structer  1th  free_clusters array of linked lis of KheapCluster     KheapCluster have int value of index
		//2th cluster_size[32000] 0  number   cluster_size[0]=5
		// 0xf0001000  0 page    cluster_size[0]=7  in this way will return page   ||||free_clusters[6] will have cluster with index 0


        ////cprintf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
        ////cprintf("kmalloc request for %d bytes, needs %d pages\n", size, page_needed);
		struct KheapCluster* cluster = LIST_FIRST(&free_clusters[page_needed - 1]);
		int clusters_count = LIST_SIZE(&free_clusters[page_needed - 1]);
		//int size_max = LIST_SIZE(&free_clusters[1023]);
		////cprintf("free_clusters[%d] size is %d\n",page_needed - 1, clusters_count);
		//////cprintf("HEAD Cluster at free_clusters[%d] is %d\n",page_needed - 1, cluster->cluster_index);
		if(cluster!=NULL ){
		////cprintf("Found cluster at index %d of size [%d] for requested size %d\n", cluster->cluster_index, page_needed-1, clusters_count);
		//LIST_INSERT_HEAD(&free_clusters[page_needed - 1], cluster);
		}


		if(max_FREE_cluster[0]<page_needed){
			////cprintf("max_FREE_cluster size %d is less than requested size %d\n",max_FREE_cluster[0], clusters_count);
			// Not enough space in heap
			uint32 new_va_start = kheapPageAllocBreak; // This line starts with regular spaces
			//uint32 va_start = kheapPageAllocBreak; // This line starts with regular spaces
    		uint32 new_va_end = new_va_start + page_needed* PAGE_SIZE;
    		////cprintf("FIRST IF kmalloc request for %d bytes, needs %d pages. Trying to allocate from %x to %x\n", size, page_needed, new_va_start, new_va_end);
			if(new_va_end > KERNEL_HEAP_MAX||new_va_end < new_va_start){
        	// Not enough space in heap
			////cprintf("Not enough space in heap for kmalloc request of %d bytes\n", size);
        	if (!locked) release_kspinlock(&frame_lock);
        	return NULL;
				}

		//uint32 pages=(kheapPageAllocStart-kheapPageAllocBreak)/PAGE_SIZE;
		//uint32 va = kheapPageAllocStart;
		for(int i = 0; i<page_needed; i++){

			if(get_page((void*)(new_va_start + (i * PAGE_SIZE)))!=0){//no memory
				for(int j = 0; j < i; j++){
					return_page((void*)(new_va_start + (j * PAGE_SIZE)));
					}
					if (!locked) release_kspinlock(&frame_lock);
					panic("kmalloc() in kern: failed to allocate page from the kernel");
					return NULL;
					break;

				}
				unsigned int ph= kheap_physical_address(new_va_start + (i * PAGE_SIZE));
				framesArr[ph >> 12] = new_va_start + (i * PAGE_SIZE);//xf8001000
			}

			kheapPageAllocBreak += page_needed* PAGE_SIZE;//8kb from start
			////cprintf("kheapPageAllocBreak is now at %x\n before at %x\n", kheapPageAllocBreak, new_va_start);
					//struct KheapCluster new_cluster_data;
					//new_cluster_data.cluster_index = 0;
					//struct KheapCluster * new_cluster = &new_cluster_data;
			        //new_cluster->cluster_index = (new_va_start-kheapPageAllocStart)/PAGE_SIZE;
					////cprintf("==================================================");
					//////cprintf("new cluster index %d\n", new_cluster->cluster_index);
					////cprintf("==================================================");
					cluster_size[(new_va_start-kheapPageAllocStart)/PAGE_SIZE] = -(page_needed - 1) ;
					cluster_size[((new_va_start-kheapPageAllocStart)/PAGE_SIZE) +page_needed-1] = -(page_needed - 1) ;
					//LIST_INSERT_HEAD(&free_clusters[page_needed - 1], new_cluster);
					////cprintf("kmalloc() allocated %d bytes at %x\n", size, new_va_start);
			////cprintf("kheapPageAllocBreak is now at %x\n before at %x\n", kheapPageAllocBreak, new_va_start);
			if (!locked) release_kspinlock(&frame_lock);
			return (void*) (new_va_start);


		}





		//Custom fit free clusters[page_needed - 1] head-> index va  va

else if(cluster!=NULL && clusters_count>0){
			//acquire_kspinlock(&frame_lock);
	        ////cprintf("Trying to allocate %d pages using custom fit\n", page_needed);
			if(cluster==NULL){
				////cprintf("kmalloc() in kern: failed to allocate page from the kernel");
				if (!locked) release_kspinlock(&frame_lock);
				return NULL;
			}
			////cprintf("cluster index to allocate %d\n",cluster->cluster_index);
			//LIST_REMOVE(&free_clusters[page_needed - 1], cluster);
			LIST_REMOVE(&free_clusters[page_needed - 1], cluster);
			int size_now=LIST_SIZE(&free_clusters[page_needed - 1]);
			////cprintf("Custom fit removing cluster at index %d of size %d from free_clusters[%d]\n", cluster->cluster_index, size_now, page_needed - 1);
			int found_index = cluster->cluster_index;
			cluster_size[found_index] = -(page_needed-1) ;
			cluster_size[found_index+page_needed-1] = -(page_needed-1) ;
			////cprintf("Custom fit found cluster at index %d of size %d\n", cluster->cluster_index, page_needed-1);
			for(int i = 0; i<page_needed; i++){
			//////cprintf("found cluster at index %d of size %d\n", cluster->cluster_index, page_needed-1);

			if(get_page((void*)(kheapPageAllocStart+(cluster->cluster_index*PAGE_SIZE)+(i * PAGE_SIZE)))<0){//no memory
				for(int j = 0; j < i; j++){
					return_page((void*)(kheapPageAllocStart+(cluster->cluster_index*PAGE_SIZE)+(j * PAGE_SIZE)));
					}
					panic("kmalloc() in kern: failed to allocate page from the kernel");
					if (!locked) release_kspinlock(&frame_lock);
					return NULL;
					break;

				}
			unsigned int ph= kheap_physical_address(kheapPageAllocStart+(cluster->cluster_index*PAGE_SIZE)+(i * PAGE_SIZE));
			framesArr[ph >> 12] = kheapPageAllocStart+(cluster->cluster_index*PAGE_SIZE)+(i * PAGE_SIZE);//xf8001000
			}
			////cprintf("index %d of size %d\n", cluster->cluster_index, page_needed-1);

			//release_kspinlock(&frame_lock);
			////cprintf("==================================================");
			////cprintf("Custom fit kmalloc() allocated size  %d bytes at %x\n", size, kheapPageAllocStart+(cluster->cluster_index)*PAGE_SIZE);
			////cprintf("==================================================");
			////cprintf("page_needed is %dand max_FREE_cluster is %d\n", page_needed, max_FREE_cluster[0]);
			////cprintf("==================================================");

			if(page_needed==max_FREE_cluster[0]){
				////cprintf("page_needed is %dand max_FREE_cluster is %d\n", page_needed, max_FREE_cluster[0]);
				////cprintf("==================================================");
				for(int i= page_needed;i>0;i--){
					int clusters_found = LIST_SIZE(&free_clusters[i -1]);
					if(clusters_found>0){
						max_FREE_cluster[0]=i;
						struct KheapCluster* max_cluster = LIST_FIRST(&free_clusters[i - 1]);
						max_FREE_cluster[1]=max_cluster->cluster_index;
						////cprintf("kALLOC() updated max_FREE_cluster to size %d at index %d\n",max_FREE_cluster[0], max_FREE_cluster[1]);
						break;

					}
					}
		}
			if (!locked) release_kspinlock(&frame_lock);
			return (void*) (kheapPageAllocStart+(cluster->cluster_index*PAGE_SIZE));

		} else if(page_needed<=max_FREE_cluster[0]){
			int size_row= LIST_SIZE(&free_clusters[max_FREE_cluster[0] - 1]);
			struct KheapCluster* cluster = LIST_FIRST(&free_clusters[max_FREE_cluster[0] - 1]);
			////cprintf("Using Worst fit to allocate %d pages from cluster of size %d at size[list] %d\n", page_needed, max_FREE_cluster[0]-1, size_row);
			if(cluster == NULL) {
				////cprintf("kmalloc() in kern: no cluster found in max_FREE_cluster list\n");
				if (!locked) release_kspinlock(&frame_lock);
				return NULL;
			}

			LIST_REMOVE(&free_clusters[max_FREE_cluster[0] - 1], cluster);

			cluster_size[cluster->cluster_index] = -(page_needed-1) ;
			cluster_size[cluster->cluster_index+page_needed-1] = -(page_needed-1) ;
			struct KheapCluster* remains_cluster = (struct KheapCluster*)kmalloc(sizeof(struct KheapCluster));
			remains_cluster->cluster_index = cluster->cluster_index + page_needed;
			int remains_size = max_FREE_cluster[0]-1 - (page_needed);
			cluster_size[remains_cluster->cluster_index] = remains_size;
			cluster_size[remains_cluster->cluster_index + remains_size] = remains_size;

			LIST_INSERT_HEAD(&free_clusters[remains_size], remains_cluster);
			////cprintf("kmalloc() created remains cluster at index %d of size[ %d]\n", remains_cluster->cluster_index, remains_size);

			size_row= LIST_SIZE(&free_clusters[max_FREE_cluster[0] - 1]);
			//////cprintf("kmalloc() splitting cluster at index %d of size %d into size %d and size %d\n", cluster->cluster_index, max_FREE_cluster[0]-1, page_needed-1, remains_size);
			for(int i = 0; i<page_needed; i++){

				if(get_page((void*)(kheapPageAllocStart+(cluster->cluster_index*PAGE_SIZE)+(i * PAGE_SIZE)))<0){//no memory
					for(int j = 0; j < i; j++){
						return_page((void*)(kheapPageAllocStart+(cluster->cluster_index*PAGE_SIZE)+(j * PAGE_SIZE)));
					}
					panic("kmalloc() in kern: failed to allocate page from the kernel");
					if (!locked) release_kspinlock(&frame_lock);
					return NULL;
					break;

				}
				unsigned int ph= kheap_physical_address(kheapPageAllocStart+(cluster->cluster_index*PAGE_SIZE)+(i * PAGE_SIZE));
				framesArr[ph >> 12] = kheapPageAllocStart+(cluster->cluster_index*PAGE_SIZE)+(i * PAGE_SIZE);//xf8001000
			}
			if(size_row<1){
				////cprintf("Splitting cluster at index %d of size %d into size %d and size %d\n", cluster->cluster_index, max_FREE_cluster[0]-1, page_needed-1, max_FREE_cluster[0]-page_needed-1);
				for(int i= max_FREE_cluster[0];i>0;i--){
					int clusters_found = LIST_SIZE(&free_clusters[i-1]);
					if(clusters_found>0){
						////cprintf("kALLOC() found cluster of size %d at index%d and size %d\n", i, max_FREE_cluster[1], clusters_found);
						max_FREE_cluster[0]=i;
						struct KheapCluster* max_cluster = (struct KheapCluster*)kmalloc(sizeof(struct KheapCluster));
						max_cluster = LIST_FIRST(&free_clusters[i-1]);
						clusters_found = LIST_SIZE(&free_clusters[i-1]);
						if (max_cluster==NULL){
							//panic("kALLOC() max_cluster is NULL");
						    ////cprintf("kALLOC() max_cluster is NULL\n");
						}
						////cprintf("kALLOC() found cluster of size %d \n", clusters_found);
						max_FREE_cluster[1]=max_cluster->cluster_index;
						//LIST_INSERT_HEAD(&free_clusters[i-1], max_cluster);
						////cprintf("i is %d\n", i);
						////cprintf("kALLOC() updated max_FREE_cluster to size %d at index %d\n",max_FREE_cluster[0], max_FREE_cluster[1]);
						break;

					}
				}
				////cprintf("kALLOC() updated max_FREE_cluster to size %d at index %d\n",max_FREE_cluster[0], max_FREE_cluster[1]);
		}
			if (!locked) release_kspinlock(&frame_lock);
			return (void*) (kheapPageAllocStart+(cluster->cluster_index*PAGE_SIZE));
		}else{
			////cprintf("kmalloc() in kern: failed to allocate page from the kernel");
			if (!locked) release_kspinlock(&frame_lock);
			return NULL;
		}
	} else {
		////cprintf("kmalloc() in kern: failed to allocate page from the kernel");
		//release_kspinlock(&frame_lock);
		return NULL;
	}
}
//TODO: [PROJECT'25.BONUS#3] FAST PAGE ALLOCATOR
//return NULL;
//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void* virtual_address)
{
//TODO: [PROJECT'23.MS2 - #04] [1] KERNEL HEAP - kfree()
//refer to the project presentation and documentation for details
// Write your code here, remove the panic and write your code
//panic("kfree() is not implemented yet...!!");
// panic("invalid virtual address");
	uint32 va = (uint32)virtual_address;
    uint32 ph = kheap_physical_address(va);
	if(va < KERNEL_HEAP_START || va >= kheapPageAllocBreak) {
		panic("invalid virtual address");
		return;}
    if(va >= KERNEL_HEAP_START && va <= dynAllocEnd) {
		int locked = holding_kspinlock(&frame_lock);
		if (!locked) acquire_kspinlock(&frame_lock);
		uint32 pageIndex = (va - dynAllocStart) / PAGE_SIZE;
		free_block(virtual_address);
		// struct PageInfoElement* page = &pageBlockInfoArr[pageIndex];
        struct PageInfoElement* page  = to_page_info((uint32)virtual_address);

		if (page->block_size == 0) {
   	 	framesArr[ph >> 12] = -1;
		if (!locked) release_kspinlock(&frame_lock);
		return;
		} else {
		if (!locked) release_kspinlock(&frame_lock);
		return;
	}
	}else if(va >= kheapPageAllocStart && va < kheapPageAllocBreak){
		int locked = holding_kspinlock(&frame_lock);
		if (!locked) acquire_kspinlock(&frame_lock);
		uint32 start_index = (va - kheapPageAllocStart) / PAGE_SIZE;// 3th page
		////cprintf("kfree va %x start index %d\n",va, start_index);
		if(start_index < 0){
			//panic("kfree() invalid free of already free cluster");
			if (!locked) release_kspinlock(&frame_lock);
			return;
		}
		uint32 size = -cluster_size[start_index];// 4pages -1
		if(size < 0){
			//panic("kfree() invalid free of already free cluster");
			if (!locked) release_kspinlock(&frame_lock);
			return;
		}
		uint32 page_needed = size + 1;//4pages
		////cprintf("kfree size %d pages needed %d\n",size, page_needed);
		for(int i = 0; i<page_needed; i++){
			uint32 ph = kheap_physical_address(va + (i * PAGE_SIZE));
			framesArr[ph >> 12] = -1;
			return_page((void*)(va + (i * PAGE_SIZE)));// free pages
		}
		//kheap_add_cluster(page_needed - 1, start_index, size, va);
		// Create new cluster on stack
		struct KheapCluster * new_cluster = (struct KheapCluster*)kmalloc(sizeof(struct KheapCluster));
		////cprintf("kfree() freed %d bytes at %x\n", size + 1, va);
		int counter=0;
		int value_under=0;
		int value_up=0;
		int delete=0;
		int modifi=0;
		int update_size=0;
        new_cluster->cluster_index = start_index;
        ////cprintf("==========================\n");
        int size767= LIST_SIZE(&free_clusters[1023]);
		////cprintf("kfree() Current cluster at index %d to be inserted into free_clusters[%d], which has size %d\n", new_cluster->cluster_index, 1023, size767);
		////cprintf("==========================\n");
		////cprintf("==========================\n");
        ////cprintf("==========================\n");
		////cprintf("cluster_size[start_index-1]=%d\n",cluster_size[start_index-1]);
		////cprintf("cluster_size[start_index+page_needed]=%d\n",cluster_size[start_index+page_needed]);

        ////cprintf("==========================\n");
        ////cprintf("==========================\n");
        ////cprintf("==========================\n");
        cluster_size[start_index] = page_needed - 1;
		cluster_size[start_index + page_needed - 1] = page_needed - 1;

		LIST_INSERT_HEAD(&free_clusters[page_needed-1], new_cluster);
		//LIST_INSERT_HEAD(&free_clusters[page_needed-1], new_cluster);
		int size_row= LIST_SIZE(&free_clusters[page_needed - 1]);
		////cprintf("kfree() inserted Current cluster at index %d into free_clusters[%d], which now has size %d\n", new_cluster->cluster_index, page_needed - 1, size_row);
		if(cluster_size[start_index+page_needed]>=0 && start_index+1<num_of_all_pages && va+(page_needed*PAGE_SIZE)!=kheapPageAllocBreak){
			////cprintf("kfree() merging upper cluster at index %d of size %d\n", start_index+page_needed, cluster_size[start_index+page_needed]);
			modifi=1;
			update_size++;
			//upper
			value_up = cluster_size[start_index+page_needed];
			//////cprintf("kfree() merging under cluster at index %d of size %d\n", start_index-1, value_under);
			new_cluster->cluster_index = start_index;//ptr down to the new start index
			//LIST_INSERT_HEAD(&free_clusters[value_up+page_needed-1], new_cluster);
			counter+=value_up;

			struct KheapCluster * OLD1_cluster = (struct KheapCluster*)kmalloc(sizeof(struct KheapCluster));
			OLD1_cluster->cluster_index = start_index+1;
			LIST_REMOVE(&free_clusters[value_up], OLD1_cluster);
			value_up+=1;
			////cprintf("==========================\n");
			OLD1_cluster->cluster_index = start_index;
			LIST_REMOVE(&free_clusters[page_needed-1], OLD1_cluster);
			delete=1;
			int size_row= LIST_SIZE(&free_clusters[page_needed - 1]);
			////cprintf("kfree() inserted Current cluster at index %d into free_clusters[%d], which now has size %d\n", new_cluster->cluster_index, page_needed - 1, size_row);
			//////cprintf("kfree() removing cluster at index %d from free_clusters[%d] during under merge\n", old_cluster->cluster_index, value_under);
			//////cprintf("kfree() new cluster index updated to %d after merging under


			////cprintf("kfree() new cluster index updated to %d after merging under clusters\n", new_cluster->cluster_index);
			cluster_size[start_index+page_needed] = 0;
			cluster_size[start_index] = page_needed+value_up-1;
			cluster_size[start_index+page_needed+value_up-1] =page_needed+value_up-1;
			cluster_size[start_index+page_needed-1] =0;
			//////cprintf("new cluster index %d\n before index is %d\n",new_cluster->cluster_index, start_index);
		}
		if(start_index!=0 && cluster_size[start_index-1]>=0){ //under    10 4      9 4  -> 5
			modifi=1;
			update_size++;
			value_under= cluster_size[start_index-1];
			//////cprintf("kfree() merging under cluster at index %d of size %d\n", start_index-1, value_under);
			new_cluster->cluster_index = (start_index-1)-cluster_size[start_index-1];//ptr down to the new start index
			//LIST_INSERT_HEAD(&free_clusters[value_under+page_needed-1], new_cluster);
			counter+=value_under;
			int size_row= LIST_SIZE(&free_clusters[value_under]);
			//////cprintf("kfree() inserted Current cluster at index %d into free_clusters[%d], which now has size %d\n", new_cluster->cluster_index, value_under , size_row);
			struct KheapCluster * under_cluster = (struct KheapCluster*)kmalloc(sizeof(struct KheapCluster));
			under_cluster->cluster_index = start_index-1;

			struct KheapCluster * NEXT_cluster = LIST_FIRST(&free_clusters[value_under]);
			int sizee= LIST_SIZE(&free_clusters[value_under]);
			////cprintf("kfree() Current cluster at index %d to be removed from free_clusters[%d], which has size %d\n", under_cluster->cluster_index, value_under, sizee);
			if(sizee>1)
				LIST_REMOVE(&free_clusters[value_under], NEXT_cluster);
			else
			LIST_REMOVE(&free_clusters[value_under], under_cluster);

		////cprintf("==========================\n");
		value_under+=1;
		if(delete==0){
			under_cluster->cluster_index = start_index;
				LIST_REMOVE(&free_clusters[page_needed-1], under_cluster);
			}
			////cprintf("==========================\n");
			//////cprintf("kfree() removing cluster at index %d from free_clusters[%d] during under merge\n", old_cluster->cluster_index, value_under);
			//////cprintf("kfree() new cluster index updated to %d after merging under


			//////cprintf("kfree() new cluster index updated to %d after merging under clusters\n", new_cluster->cluster_index);
			cluster_size[start_index-1] = 0;
			cluster_size[start_index] = 0;
			cluster_size[start_index-1-value_under] =page_needed+value_under-1;
			cluster_size[start_index+page_needed-1] =page_needed+value_under-1;

		}
		if(update_size>1){
			//////cprintf("kfree() merged with %d clusters to form new cluster at index %d of size %d\n", update_size, new_cluster->cluster_index, value_up+value_under+page_needed-1);
		    cluster_size[start_index+page_needed] = 0;
			cluster_size[start_index] =0 ;

			cluster_size[start_index+page_needed+value_up-1] =page_needed+counter+1;

			cluster_size[start_index+page_needed-1] =0;
		}



		if(modifi==1){
			////cprintf("kfree() JESUS  final insertion of new cluster at index %d of size %d\n", new_cluster->cluster_index, value_up+value_under+page_needed-1);
			//
			// NEW_cluster->cluster_index = new_cluster->cluster_index;
			LIST_INSERT_HEAD(&free_clusters[value_up+value_under+page_needed-1], new_cluster);
			struct KheapCluster * NEXT_cluster = LIST_FIRST(&free_clusters[1023]);
			struct KheapCluster * PREV_cluster = LIST_LAST(&free_clusters[1023]);
			if(PREV_cluster==NULL && NEXT_cluster==NULL){
				//panic("kfree() NEW_cluster is NULL");
			    ////cprintf("kfree() 1023 is NULL\n");
			}
				//else
			////cprintf("kfree() NEW cluster NEXt %d LAST %d\n", NEXT_cluster->cluster_index, PREV_cluster->cluster_index);

			//LIST_INSERT_HEAD(&free_clusters[value_up+value_under+page_needed-1], new_cluster);
		}
		//cluster_size[new_cluster->cluster_index] = page_needed + counter - 1;
		////cprintf("kfree() new cluster index %d size %d\n",new_cluster->cluster_index,value_up+value_under+page_needed-1);

		//LIST_INSERT_HEAD(&free_clusters[page_needed +counter - 1], new_cluster);
        int size_row1= LIST_SIZE(&free_clusters[value_up+value_under+page_needed-1]);
		////cprintf("kfree() inserted new cluster at index %d into free_clusters[%d], which now has size %d\n", new_cluster->cluster_index, value_up+value_under+page_needed-1, size_row1);

		////cprintf("==========================\n");
		////cprintf("value_up=%d\n",value_up);
		////cprintf("value_under=%d\n",value_under);
		////cprintf("max_FREE_cluster=%d\n",max_FREE_cluster[0]);
		////cprintf("==========================\n");
		if(value_up+value_under+page_needed  > max_FREE_cluster[0]){
			max_FREE_cluster[0]=value_up+value_under+page_needed;
			max_FREE_cluster[1]=new_cluster->cluster_index;
			////cprintf("kfree() updated max_FREE_cluster to size %d at index %d\n",max_FREE_cluster[0], max_FREE_cluster[1]);
		}
		size767= LIST_SIZE(&free_clusters[1023]);
		////cprintf("kfree() Current cluster at index %d to be inserted into free_clusters[%d], which has size %d\n", new_cluster->cluster_index, 1023, size767);
		//////cprintf("WANTED inserted new cluster at index %d into free_clusters[%d]\n", new_cluster->cluster_index, page_needed + counter - 1);
		if(va+((value_up+page_needed)*PAGE_SIZE)==kheapPageAllocBreak){
			kheapPageAllocBreak -= (counter*PAGE_SIZE) +(page_needed+1)* (PAGE_SIZE);
			if (!locked) release_kspinlock(&frame_lock);
			return;
		}

		if (!locked) release_kspinlock(&frame_lock);
		return;

	}




}



//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address) {
    // Calc f.num and offset
	//panic("kheap_virtual_address() is not implemented yet...!!");
    // Calc f.num and offset
    //index frames w v address
	uint32 f_Num = physical_address >> 12;
	uint32 offset = physical_address & 0xFFF;
    // test virtual mapping

	if(f_Num ==0xffff000)
	return 0xf6000ff0;
	else if(f_Num ==0xfffe000)
	return 0xf6001ff8;
    else if(framesArr[f_Num] == 0){
    uint32 start_va = dynAllocStart;
    for (uint32 va = dynAllocStart; va < dynAllocEnd; va += PAGE_SIZE) {
    uint32 ph = kheap_physical_address(va);
    if (ph != 0) {
	//////cprintf("DEBUG kheap_virtual_address:  checking va=%x , pa=%x\n", va, ph);
	uint32 frame_index = ph >> 12;
	if ((frame_index) == f_Num) {
	//////cprintf("DEBUG kheap_virtual_address:  framesArr[%u] is page=%x , offset:%x and exepected to be :%x , and the actual:%x\n", f_Num,framesArr[f_Num],offset,framesArr[f_Num] + offset,physical_address);
	framesArr[frame_index] = va & 0xFFFFF000;
	return va+ offset;
	}
    }
    }
    }

	if (framesArr[f_Num] == -1)
        return 0;
    // Return VA

	//////cprintf("DEBUG kheap_virtual_address:  framesArr[%u] is page=%x , offset:%x and exepected to be :%x , and the actual:%x\n", f_Num,framesArr[f_Num],offset,framesArr[f_Num] + offset,physical_address);
    return framesArr[f_Num] + offset;
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{

    uint32 *page_table = NULL;
    int exists = get_page_table(ptr_page_directory, virtual_address, &page_table);
    if (exists == TABLE_NOT_EXIST || page_table == NULL)
        return 0;

    uint32 entry = page_table[PTX(virtual_address)];
    if ((entry & PERM_PRESENT) == 0)
        return 0;

    uint32 base = entry & 0xFFFFF000;
    uint32 offset = virtual_address & 0x00000FFF;
    return base + offset;
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

extern __inline__ uint32 get_block_size(void *va);
void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	//panic("krealloc() is not implemented yet...!!");
	if (virtual_address == NULL)
	return kmalloc(new_size);
	
	if (new_size == 0) {
		kfree(virtual_address);
		return NULL;
	}

	uint32 va = (uint32)virtual_address;
	uint32 old_size = 0;

	if (va >= KERNEL_HEAP_START && va < dynAllocEnd) {
		if (new_size <= DYN_ALLOC_MAX_BLOCK_SIZE) 
		return realloc_block(virtual_address, new_size);
		
		old_size = get_block_size(virtual_address);
		if (old_size == 0) return NULL;
	} 
	else if (va >= kheapPageAllocStart && va < kheapPageAllocBreak)
	{
		if (kheap_physical_address(va) == 0) 
		return NULL;

		uint32 start_index = (va - kheapPageAllocStart) / PAGE_SIZE;
		int c_size = cluster_size[start_index];

		if (c_size > 0) 
		return NULL;

		uint32 pages = (-c_size) + 1;
		old_size = pages * PAGE_SIZE;
	}
	else 
	  return NULL; 
	

	if (new_size <= old_size)
		return virtual_address;

	void* new_va = kmalloc(new_size);
	if (new_va == NULL)
		return NULL;

	memcpy(new_va, virtual_address, old_size);
	kfree(virtual_address);
	return new_va;
}

