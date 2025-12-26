#ifndef FOS_KERN_KHEAP_H_
#define FOS_KERN_KHEAP_H_

#ifndef FOS_KERNEL
# error "This is a FOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>
#include <kern/conc/kspinlock.h>

extern struct kspinlock frame_lock;
extern struct Env_Queue alloc_wait_queue;


/*2017*/
//Values for user heap placement strategy
#define KHP_PLACE_CONTALLOC 0x0
#define KHP_PLACE_FIRSTFIT 	0x1
#define KHP_PLACE_BESTFIT 	0x2
#define KHP_PLACE_NEXTFIT 	0x3
#define KHP_PLACE_WORSTFIT 	0x4
#define KHP_PLACE_CUSTOMFIT 0x5

//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 Page Alloc Limits [GIVEN]
uint32 kheapPageAllocStart ;
uint32 kheapPageAllocBreak ;
uint32 kheapPlacementStrategy;

/*2025*/ //Replaced by setter & getter function
static inline void set_kheap_strategy(uint32 strategy){kheapPlacementStrategy = strategy;}
static inline uint32 get_kheap_strategy(){return kheapPlacementStrategy ;}

//==================================================================================
// KERNEL HEAP CLUSTER MANAGEMENT WITH FOS LINKED LISTS
//==================================================================================
#define NUM_KHEAP_ROWS 32766

/* Define a cluster element (column in a row) */
struct KheapCluster {
    int cluster_index;      /* Index into size_arr[] and va_arr[] */

    LIST_ENTRY(KheapCluster) prev_next_info;  /* Linkage for doubly-linked list */
};

/* Define list type for managing clusters per row */
LIST_HEAD(KheapClusterList, KheapCluster);

/* Array of 32766 linked lists: each row represents free clusters for that page */
struct KheapClusterList free_clusters[NUM_KHEAP_ROWS];

/* Tracking arrays for cluster metadata */
int cluster_size[NUM_KHEAP_ROWS];          /* Virtual address of each cluster (indexed by cluster_index) */

int max_FREE_cluster[2];//size and index
uint32 framesArr [1000000];// in dynamic   []
//==================================================================================
// HELPER FUNCTIONS FOR KHEAP CLUSTER MANAGEMENT
//==================================================================================

/* Initialize all cluster lists (call once during kheap_init) */
static inline void kheap_clusters_init() {
    for (int i = 0; i < NUM_KHEAP_ROWS; i++) {
        LIST_INIT(&free_clusters[i]);
    }
}


//int numOfPages[32722];
//unsigned int vmS[32722];
//***********************************
void kheap_init();

void* kmalloc(unsigned int size);
void kfree(void* virtual_address);
void *krealloc(void *virtual_address, unsigned int new_size);

unsigned int kheap_virtual_address(unsigned int physical_address);
unsigned int kheap_physical_address(unsigned int virtual_address);

int numOfKheapVACalls ;


#endif // FOS_KERN_KHEAP_H_
