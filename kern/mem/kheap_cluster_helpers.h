#ifndef FOS_KERN_KHEAP_CLUSTER_HELPERS_H_
#define FOS_KERN_KHEAP_CLUSTER_HELPERS_H_

#ifndef FOS_KERNEL
# error "This is a FOS kernel header; user programs should not #include it"
#endif

#include "kheap.h"
#include <inc/queue.h>
#include <inc/lib.h>

//==================================================================================
// HELPER FUNCTIONS FOR KHEAP CLUSTER MANAGEMENT
//==================================================================================

/* Initialize all cluster lists (call once during kheap_init) */
static inline void kheap_clusters_init() {
    for (int i = 0; i < NUM_KHEAP_ROWS; i++) {
        LIST_INIT(&free_clusters[i]);
    }
}

/* Add a new cluster to a row's free list */
static inline void kheap_add_cluster(int row, int cluster_idx, int size, uint32 va) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: kheap_add_cluster - row %d out of bounds\n", row);
        return;
    }
    
    struct KheapCluster *new_cluster = malloc(sizeof(struct KheapCluster));
    if (new_cluster == NULL) {
        cprintf("ERROR: kheap_add_cluster - malloc failed\n");
        return;
    }
    
    new_cluster->cluster_index = cluster_idx;
    new_cluster->size = size;
    new_cluster->virtual_address = va;
    
    LIST_INSERT_HEAD(&free_clusters[row], new_cluster);
}

/* Find and remove a cluster from a row */
static inline struct KheapCluster* kheap_remove_cluster(int row, int cluster_idx) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: kheap_remove_cluster - row %d out of bounds\n", row);
        return NULL;
    }
    
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        if (elem->cluster_index == cluster_idx) {
            LIST_REMOVE(&free_clusters[row], elem);
            return elem;  /* Caller responsible for freeing */
        }
    }
    
    return NULL;  /* Not found */
}

/* Get a cluster from a row without removing it */
static inline struct KheapCluster* kheap_get_cluster(int row, int cluster_idx) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: kheap_get_cluster - row %d out of bounds\n", row);
        return NULL;
    }
    
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        if (elem->cluster_index == cluster_idx) {
            return elem;
        }
    }
    
    return NULL;  /* Not found */
}

/* Get the first free cluster in a row (best fit / first fit) */
static inline struct KheapCluster* kheap_get_first_free_cluster(int row) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: kheap_get_first_free_cluster - row %d out of bounds\n", row);
        return NULL;
    }
    
    return LIST_FIRST(&free_clusters[row]);
}

/* Get the total number of free clusters in a row */
static inline int kheap_row_size(int row) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: kheap_row_size - row %d out of bounds\n", row);
        return -1;
    }
    
    return LIST_SIZE(&free_clusters[row]);
}

/* Check if a row has any free clusters */
static inline int kheap_row_is_empty(int row) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: kheap_row_is_empty - row %d out of bounds\n", row);
        return -1;
    }
    
    return LIST_EMPTY(&free_clusters[row]);
}

/* Traverse and print all clusters in a row (for debugging) */
static inline void kheap_print_row(int row) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: kheap_print_row - row %d out of bounds\n", row);
        return;
    }
    
    cprintf("Row %d clusters (%d total): ", row, LIST_SIZE(&free_clusters[row]));
    
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        cprintf("[idx:%d size:%d va:%x] ", elem->cluster_index, elem->size, elem->virtual_address);
    }
    
    cprintf("\n");
}

/* Free all clusters in a row (cleanup) */
static inline void kheap_clear_row(int row) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: kheap_clear_row - row %d out of bounds\n", row);
        return;
    }
    
    struct KheapCluster *elem, *next;
    
    for (elem = LIST_FIRST(&free_clusters[row]); elem != NULL; elem = next) {
        next = LIST_NEXT(elem);
        LIST_REMOVE(&free_clusters[row], elem);
        free(elem);
    }
}

#endif // FOS_KERN_KHEAP_CLUSTER_HELPERS_H_
