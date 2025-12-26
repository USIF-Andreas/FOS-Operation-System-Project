# TL;DR: The Simplified Approach

## What Changed in kheap.h

### BEFORE (Old Complex Way):
```c
struct KheapCluster {
    int cluster_index;
    int size;               // ← Stored in struct
    uint32 virtual_address; // ← Stored in struct
    LIST_ENTRY(KheapCluster) prev_next_info;
};
```

### AFTER (New Simple Way):
```c
struct KheapCluster {
    int cluster_index;      // ← Use this as array index!
    LIST_ENTRY(KheapCluster) prev_next_info;
};

// Store size and VA in separate arrays
int cluster_size[NUM_KHEAP_ROWS];
uint32 cluster_va[NUM_KHEAP_ROWS];
```

✅ **Already updated in kheap.h!**

---

## Why This Is Better

| Problem | Solution |
|---------|----------|
| How many pages to unmap? | `cluster_size[idx] / PAGE_SIZE` |
| Which virtual address? | `cluster_va[idx]` |
| Store in arrays once | Access via index multiple times |

---

## The 3 Key Functions

### 1. Initialize
```c
void kheap_clusters_init(void) {
    for (int i = 0; i < NUM_KHEAP_ROWS; i++) {
        LIST_INIT(&free_clusters[i]);
        cluster_size[i] = 0;
        cluster_va[i] = 0;
    }
}
```

### 2. Add Cluster
```c
void kheap_add_cluster(int row, int cluster_idx, int size, uint32 va) {
    // Store metadata
    cluster_size[cluster_idx] = size;
    cluster_va[cluster_idx] = va;
    
    // Create node with just the index
    struct KheapCluster *node = malloc(sizeof(struct KheapCluster));
    node->cluster_index = cluster_idx;
    
    // Add to linked list
    LIST_INSERT_HEAD(&free_clusters[row], node);
}
```

### 3. Remove Cluster
```c
struct KheapCluster* kheap_remove_cluster(int row, int cluster_idx) {
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        if (elem->cluster_index == cluster_idx) {
            LIST_REMOVE(&free_clusters[row], elem);
            return elem;
        }
    }
    return NULL;
}
```

---

## In kmalloc()

```c
// After allocating pages at new_va_start with new_size bytes:

int row = (new_size / PAGE_SIZE) - 1;
static int cluster_counter = 0;

acquire_kspinlock(&frame_lock);
kheap_add_cluster(row, cluster_counter, new_size, new_va_start);
cluster_counter++;
release_kspinlock(&frame_lock);

return (void*)new_va_start;
```

---

## In kfree()

```c
uint32 va = (uint32)virtual_address;

acquire_kspinlock(&frame_lock);

for (int row = 0; row < NUM_KHEAP_ROWS; row++) {
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        // Find cluster with matching VA
        if (cluster_va[elem->cluster_index] == va) {
            int idx = elem->cluster_index;
            int size = cluster_size[idx];  // ← Get size from array
            
            // Unmap all pages
            for (int j = 0; j < size / PAGE_SIZE; j++) {
                unmap_frame(ptr_page_directory, va + (j * PAGE_SIZE));
            }
            
            // Remove from list and clean up
            LIST_REMOVE(&free_clusters[row], elem);
            free(elem);
            cluster_size[idx] = 0;
            cluster_va[idx] = 0;
            release_kspinlock(&frame_lock);
            return;
        }
    }
}

release_kspinlock(&frame_lock);
```

---

## Files to Read

1. **SIMPLIFIED_APPROACH.md** - Full implementation details
2. **SIMPLIFIED_VISUAL.md** - Visual diagrams
3. Copy the 3 functions above into kheap.c
4. Use in kmalloc() and kfree() as shown

---

## Key Insight

```
cluster_index is an INDEX into two arrays:

cluster_size[cluster_index]  ← How big?
cluster_va[cluster_index]    ← Where is it?

So:
  - Linked list stores: cluster_index
  - Arrays store: size and VA
  - Lookup: cluster_size[idx], cluster_va[idx]
  - Simple!
```

Done! ✅

