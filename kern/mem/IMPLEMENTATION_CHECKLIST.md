# Complete Checklist: Using free_clusters Array

## What You Have

```c
// In kheap.h:
#define NUM_KHEAP_ROWS 32766

struct KheapCluster {
    int cluster_index;
    int size;
    uint32 virtual_address;
    LIST_ENTRY(KheapCluster) prev_next_info;
};

LIST_HEAD(KheapClusterList, KheapCluster);
struct KheapClusterList free_clusters[NUM_KHEAP_ROWS];
```

✅ **This array is READY to use!**

---

## TODO: 7 Step Implementation

### [ ] Step 1: Add Helper Functions to kheap.c

Copy-paste these 8 functions into **kheap.c** (end of file):

```c
void kheap_clusters_init(void)
void kheap_add_cluster(int row, int cluster_idx, int size, uint32 va)
struct KheapCluster* kheap_remove_cluster(int row, int cluster_idx)
struct KheapCluster* kheap_get_cluster(int row, int cluster_idx)
struct KheapCluster* kheap_get_first_cluster(int row)
int kheap_row_size(int row)
int kheap_row_is_empty(int row)
void print_all_clusters(void)  // Optional: for debugging
```

**See: `HOW_TO_USE_CLUSTERS.md` Step 2 for exact code**

---

### [ ] Step 2: Add Declarations to kheap.h

Add these declarations to **kheap.h** (before `#endif`):

```c
/* Cluster management helper functions */
void kheap_clusters_init(void);
void kheap_add_cluster(int row, int cluster_idx, int size, uint32 va);
struct KheapCluster* kheap_remove_cluster(int row, int cluster_idx);
struct KheapCluster* kheap_get_cluster(int row, int cluster_idx);
struct KheapCluster* kheap_get_first_cluster(int row);
int kheap_row_size(int row);
int kheap_row_is_empty(int row);
```

**See: `HOW_TO_USE_CLUSTERS.md` Step 3 for exact location**

---

### [ ] Step 3: Initialize in kheap_init()

In **kheap_init()** function, add this line at the end:

```c
kheap_clusters_init();  // Initialize all 32766 linked lists
```

**See: `HOW_TO_USE_CLUSTERS.md` Step 4 for exact location**

---

### [ ] Step 4: Track Allocations in kmalloc()

When you allocate pages in **kmalloc()**, add this:

```c
// After successfully allocating pages
int row = (number_of_pages - 1);  // Row based on page count
static int cluster_counter = 0;

if (row < NUM_KHEAP_ROWS) {
    kheap_add_cluster(row, cluster_counter, size, new_va_start);
    cluster_counter++;
}
```

**See: `HOW_TO_USE_CLUSTERS.md` Step 5 for full kmalloc() code**

---

### [ ] Step 5: Track Deallocations in kfree()

In **kfree()** for large allocations, add this:

```c
// Search for and remove the cluster
for (int row = 0; row < NUM_KHEAP_ROWS; row++) {
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        if (elem->virtual_address == va) {
            // Unmap pages
            for (int j = 0; j < elem->size / PAGE_SIZE; j++) {
                unmap_frame(ptr_page_directory, va + (j * PAGE_SIZE));
            }
            // Remove from list
            LIST_REMOVE(&free_clusters[row], elem);
            free(elem);
            return;
        }
    }
}
```

**See: `HOW_TO_USE_CLUSTERS.md` Step 6 for full kfree() code**

---

### [ ] Step 6: Add Spinlock Protection

Wrap all cluster operations with:

```c
acquire_kspinlock(&frame_lock);
    // Your cluster operations here
    kheap_add_cluster(...);
    kheap_remove_cluster(...);
release_kspinlock(&frame_lock);
```

**See: `HOW_TO_USE_CLUSTERS.md` for examples**

---

### [ ] Step 7: Test

Compile and test:

```bash
make clean
make
# Run with test cases that allocate/free memory
```

---

## Files to Read for Implementation

| File | Purpose |
|------|---------|
| `HOW_TO_USE_CLUSTERS.md` | **MAIN GUIDE** - Step-by-step with code |
| `VISUAL_EXAMPLE.md` | Diagrams showing what happens |
| `kheap.h` | Header with struct definitions (already done ✓) |
| `kheap.c` | Where you implement the functions |

---

## Quick Reference: The 7 Functions You Need

```c
// 1. Initialize (call once at boot)
kheap_clusters_init();

// 2. Add when allocating
kheap_add_cluster(row, cluster_idx, size, virtual_address);

// 3. Remove when deallocating
struct KheapCluster *cluster = kheap_remove_cluster(row, cluster_idx);

// 4. Find without removing
struct KheapCluster *cluster = kheap_get_cluster(row, cluster_idx);

// 5. Get first cluster (for first-fit strategy)
struct KheapCluster *cluster = kheap_get_first_cluster(row);

// 6. Check size
int num_clusters = kheap_row_size(row);

// 7. Check if empty
if (kheap_row_is_empty(row)) { ... }

// BONUS 8. Traverse
LIST_FOREACH(elem, &free_clusters[row]) {
    cprintf("Cluster %d: %d bytes at %x\n", elem->cluster_index, elem->size, elem->virtual_address);
}
```

---

## Important Reminders

⚠️ **ALWAYS USE SPINLOCK**
```c
acquire_kspinlock(&frame_lock);
kheap_add_cluster(...);  // Protected
release_kspinlock(&frame_lock);
```

⚠️ **Row Selection** - Choose based on allocation size:
```c
// Example: Row = (number_of_pages - 1)
int row = (size / PAGE_SIZE) - 1;
if (row >= NUM_KHEAP_ROWS) handle_error();
```

⚠️ **Free the Node** when removing:
```c
struct KheapCluster *elem = kheap_remove_cluster(row, idx);
if (elem) {
    use_cluster_info(elem);
    free(elem);  // Don't forget this!
}
```

⚠️ **LIST_FOREACH is NOT reentrant** - Don't nest:
```c
// ❌ WRONG: Nested LIST_FOREACH
LIST_FOREACH(a, &list1) {
    LIST_FOREACH(b, &list2) { ... }
}

// ✅ RIGHT: Use manual loop for inner
LIST_FOREACH(a, &list1) {
    struct KheapCluster *b = LIST_FIRST(&list2);
    while (b != NULL) {
        // ...
        b = LIST_NEXT(b);
    }
}
```

---

## Example: Complete kmalloc() with Cluster Tracking

```c
void* kmalloc(unsigned int size) {
    // Small allocations
    if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
        struct BlockElement* block;
        acquire_kspinlock(&frame_lock);
        block = alloc_block(size);
        if (block != NULL) {
            release_kspinlock(&frame_lock);
            return (void*) block;
        }
        release_kspinlock(&frame_lock);
        return NULL;
    }
    
    // Large allocations (pages)
    uint32 new_size = ROUNDUP(size, PAGE_SIZE);
    int number_of_pages = new_size / PAGE_SIZE;
    uint32 new_va_start = kheapPageAllocBreak;
    
    acquire_kspinlock(&frame_lock);
    
    // Allocate pages
    for (int i = 0; i < number_of_pages; i++) {
        if (alloc_page(ptr_page_directory, new_va_start + (i * PAGE_SIZE), PERM_WRITEABLE, 1) < 0) {
            // Rollback
            for (int j = 0; j < i; j++) {
                unmap_frame(ptr_page_directory, new_va_start + (j * PAGE_SIZE));
            }
            release_kspinlock(&frame_lock);
            return NULL;
        }
    }
    
    // Update break
    kheapPageAllocBreak += new_size;
    
    // TRACK CLUSTER: Store allocation info
    int row = (number_of_pages - 1);
    static int cluster_counter = 0;
    if (row < NUM_KHEAP_ROWS) {
        kheap_add_cluster(row, cluster_counter, new_size, new_va_start);
        cluster_counter++;
    }
    
    release_kspinlock(&frame_lock);
    return (void*)new_va_start;
}
```

---

## Success Criteria

✅ Your implementation is correct when:
- [ ] kheap_init() calls kheap_clusters_init()
- [ ] Every kmalloc() large allocation adds a cluster
- [ ] Every kfree() removes and unlinks the cluster
- [ ] No memory leaks (clusters are freed)
- [ ] All operations are spinlock-protected
- [ ] Code compiles without errors
- [ ] Allocation/deallocation tests pass

---

## Still Confused?

1. Read `HOW_TO_USE_CLUSTERS.md` - Step-by-step guide
2. Look at `VISUAL_EXAMPLE.md` - See exactly what happens
3. Check the "Quick Reference" above
4. Copy code from Step 5 in `HOW_TO_USE_CLUSTERS.md`

**You've got this!** 💪

