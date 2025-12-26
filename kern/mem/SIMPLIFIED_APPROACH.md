# Simplified: Using cluster_index as Array Index

## The New Structure (Updated)

```c
#define NUM_KHEAP_ROWS 32766

struct KheapCluster {
    int cluster_index;      /* Index into cluster_size[] and cluster_va[] */
    LIST_ENTRY(KheapCluster) prev_next_info;
};

LIST_HEAD(KheapClusterList, KheapCluster);

/* Main array of linked lists */
struct KheapClusterList free_clusters[NUM_KHEAP_ROWS];

/* Two tracking arrays (indexed by cluster_index) */
int cluster_size[NUM_KHEAP_ROWS];       /* cluster_size[idx] = size of cluster idx */
uint32 cluster_va[NUM_KHEAP_ROWS];      /* cluster_va[idx] = virtual address of cluster idx */
```

✅ **Much simpler!**

---

## How It Works

```
free_clusters[row] = Linked list of cluster indices

Each node stores only: cluster_index (integer)

Example:
    free_clusters[0] → [Node: idx=5] ↔ [Node: idx=7] ↔ [Node: idx=12] → NULL
    
    To get size of idx=5:  cluster_size[5]
    To get VA of idx=5:    cluster_va[5]
    To get size of idx=7:  cluster_size[7]
    To get VA of idx=7:    cluster_va[7]
```

---

## Updated Helper Functions

### 1. Initialize
```c
void kheap_clusters_init(void) {
    for (int i = 0; i < NUM_KHEAP_ROWS; i++) {
        LIST_INIT(&free_clusters[i]);
    }
    
    // Clear tracking arrays
    for (int i = 0; i < NUM_KHEAP_ROWS; i++) {
        cluster_size[i] = 0;
        cluster_va[i] = 0;
    }
}
```

### 2. Add Cluster
```c
void kheap_add_cluster(int row, int cluster_idx, int size, uint32 va) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: row %d out of bounds\n", row);
        return;
    }
    if (cluster_idx < 0 || cluster_idx >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: cluster_idx %d out of bounds\n", cluster_idx);
        return;
    }
    
    // Store metadata in tracking arrays
    cluster_size[cluster_idx] = size;
    cluster_va[cluster_idx] = va;
    
    // Create node and add to linked list
    struct KheapCluster *new_cluster = (struct KheapCluster *)malloc(sizeof(struct KheapCluster));
    if (new_cluster == NULL) {
        cprintf("ERROR: malloc failed for cluster\n");
        return;
    }
    
    new_cluster->cluster_index = cluster_idx;
    LIST_INSERT_HEAD(&free_clusters[row], new_cluster);
}
```

### 3. Remove Cluster
```c
struct KheapCluster* kheap_remove_cluster(int row, int cluster_idx) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: row %d out of bounds\n", row);
        return NULL;
    }
    
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        if (elem->cluster_index == cluster_idx) {
            LIST_REMOVE(&free_clusters[row], elem);
            return elem;  // Caller frees this
        }
    }
    
    return NULL;
}
```

### 4. Find Cluster
```c
struct KheapCluster* kheap_get_cluster(int row, int cluster_idx) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: row %d out of bounds\n", row);
        return NULL;
    }
    
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        if (elem->cluster_index == cluster_idx) {
            return elem;
        }
    }
    
    return NULL;
}
```

### 5. Get First Cluster
```c
struct KheapCluster* kheap_get_first_cluster(int row) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: row %d out of bounds\n", row);
        return NULL;
    }
    return LIST_FIRST(&free_clusters[row]);
}
```

### 6. Get Row Size
```c
int kheap_row_size(int row) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: row %d out of bounds\n", row);
        return -1;
    }
    return LIST_SIZE(&free_clusters[row]);
}
```

### 7. Check if Empty
```c
int kheap_row_is_empty(int row) {
    if (row < 0 || row >= NUM_KHEAP_ROWS) {
        cprintf("ERROR: row %d out of bounds\n", row);
        return -1;
    }
    return LIST_EMPTY(&free_clusters[row]);
}
```

---

## Usage in kmalloc()

```c
void* kmalloc(unsigned int size) {
    // ... allocate pages ...
    
    uint32 new_va_start = kheapPageAllocBreak;
    int number_of_pages = new_size / PAGE_SIZE;
    
    // ... allocate pages successfully ...
    
    // Track the cluster
    int row = (number_of_pages - 1);
    static int cluster_counter = 0;
    
    if (row < NUM_KHEAP_ROWS && cluster_counter < NUM_KHEAP_ROWS) {
        acquire_kspinlock(&frame_lock);
        
        // Store size and VA in tracking arrays
        cluster_size[cluster_counter] = new_size;
        cluster_va[cluster_counter] = new_va_start;
        
        // Add to linked list
        kheap_add_cluster(row, cluster_counter, new_size, new_va_start);
        
        cluster_counter++;
        release_kspinlock(&frame_lock);
    }
    
    return (void*)new_va_start;
}
```

---

## Usage in kfree()

```c
void kfree(void* virtual_address) {
    uint32 va = (uint32)virtual_address;
    
    // For large page allocations
    if (va >= hLimit + PAGE_SIZE && va < KERNEL_HEAP_MAX) {
        acquire_kspinlock(&frame_lock);
        
        // Find the cluster by searching VA
        for (int row = 0; row < NUM_KHEAP_ROWS; row++) {
            struct KheapCluster *elem;
            LIST_FOREACH(elem, &free_clusters[row]) {
                if (cluster_va[elem->cluster_index] == va) {
                    // Found it!
                    int cluster_idx = elem->cluster_index;
                    int size = cluster_size[cluster_idx];
                    
                    // Unmap pages
                    for (int j = 0; j < size / PAGE_SIZE; j++) {
                        unmap_frame(ptr_page_directory, va + (j * PAGE_SIZE));
                    }
                    
                    // Remove from list
                    LIST_REMOVE(&free_clusters[row], elem);
                    free(elem);
                    
                    // Clear tracking arrays
                    cluster_size[cluster_idx] = 0;
                    cluster_va[cluster_idx] = 0;
                    
                    release_kspinlock(&frame_lock);
                    return;
                }
            }
        }
        
        release_kspinlock(&frame_lock);
    }
}
```

---

## Why This Is Better

| Aspect | Before | After |
|--------|--------|-------|
| **Struct size** | 20+ bytes | ~12 bytes |
| **Size lookup** | `elem->size` | `cluster_size[idx]` |
| **VA lookup** | `elem->virtual_address` | `cluster_va[idx]` |
| **Memory waste** | Duplicated in struct | Single tracking arrays |
| **Simplicity** | Complex | Very simple |

---

## Memory Layout

```
                      KERNEL HEAP
    ┌──────────────────────────────────────────────────┐
    │  free_clusters[32766]                            │
    │    Each row = linked list of cluster indices     │
    └──────────────────────────────────────────────────┘
                      ↑
    
    ┌──────────────────────────────────────────────────┐
    │  cluster_size[32766]                             │
    │  cluster_size[0] = 4096                          │
    │  cluster_size[1] = 8192                          │
    │  cluster_size[2] = 4096                          │
    │  ...                                             │
    └──────────────────────────────────────────────────┘
    
    ┌──────────────────────────────────────────────────┐
    │  cluster_va[32766]                               │
    │  cluster_va[0] = 0xF0000000                      │
    │  cluster_va[1] = 0xF0002000                      │
    │  cluster_va[2] = 0xF0004000                      │
    │  ...                                             │
    └──────────────────────────────────────────────────┘
```

---

## Summary

✅ **Much cleaner!**

- Linked list stores only cluster_index
- Size and VA stored in separate arrays indexed by cluster_index
- Faster lookups
- Less memory per node
- Simple and straightforward

