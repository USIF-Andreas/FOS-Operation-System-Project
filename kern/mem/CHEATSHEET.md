# FREE_CLUSTERS: One-Page Cheat Sheet

## The Array (Already Defined in kheap.h ✓)

```c
struct KheapCluster {
    int cluster_index;           // Which cluster
    int size;                    // How big (bytes)
    uint32 virtual_address;      // Where (VA)
    LIST_ENTRY(KheapCluster) prev_next_info;  // Linked list
};

struct KheapClusterList free_clusters[32766];  // 32766 linked lists
```

---

## The Functions YOU Must Add to kheap.c

```c
1️⃣  void kheap_clusters_init(void)
    → Initialize all 32766 lists to empty
    → Call ONCE in kheap_init()

2️⃣  void kheap_add_cluster(int row, int idx, int size, uint32 va)
    → Add cluster to row's list
    → Call in kmalloc() after allocating

3️⃣  struct KheapCluster* kheap_remove_cluster(int row, int idx)
    → Remove and return cluster from row
    → Call in kfree() before unmapping

4️⃣  struct KheapCluster* kheap_get_cluster(int row, int idx)
    → Find without removing
    → Optional - for lookup

5️⃣  struct KheapCluster* kheap_get_first_cluster(int row)
    → Get first cluster (for first-fit)
    → Optional

6️⃣  int kheap_row_size(int row)
    → How many clusters in row?
    → Optional - for debugging

7️⃣  int kheap_row_is_empty(int row)
    → Is row empty?
    → Optional - for debugging

8️⃣  void print_all_clusters(void)
    → Print all clusters (debugging)
    → Optional - for debugging
```

---

## Usage Pattern

```c
// 🔓 ALWAYS USE SPINLOCK!

acquire_kspinlock(&frame_lock);

// ✅ DO YOUR CLUSTER OPERATIONS HERE:
// kheap_add_cluster(...)
// kheap_remove_cluster(...)
// LIST_FOREACH(...)

release_kspinlock(&frame_lock);
```

---

## In kmalloc() - Track Allocation

```c
// After allocating N pages at VA:

int row = (number_of_pages - 1);      // Which row?
static int cluster_counter = 0;        // Unique ID

acquire_kspinlock(&frame_lock);
kheap_add_cluster(row, cluster_counter, size, va);
cluster_counter++;
release_kspinlock(&frame_lock);
```

---

## In kfree() - Remove Allocation

```c
// To find and free a cluster:

acquire_kspinlock(&frame_lock);

for (int row = 0; row < NUM_KHEAP_ROWS; row++) {
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        if (elem->virtual_address == va) {
            // Unmap pages
            for (int j = 0; j < elem->size / PAGE_SIZE; j++) {
                unmap_frame(ptr_page_directory, va + (j*PAGE_SIZE));
            }
            // Remove and free
            LIST_REMOVE(&free_clusters[row], elem);
            free(elem);
            release_kspinlock(&frame_lock);
            return;
        }
    }
}

release_kspinlock(&frame_lock);
```

---

## The 7 Implementation Steps

| # | What | Where | Code |
|---|------|-------|------|
| 1 | Add helper functions | kheap.c end | 8 functions from HOW_TO_USE_CLUSTERS.md |
| 2 | Add declarations | kheap.h before #endif | 7 function prototypes |
| 3 | Initialize | kheap_init() end | `kheap_clusters_init();` |
| 4 | Track allocation | kmalloc() after pages allocated | `kheap_add_cluster(...)` |
| 5 | Track deallocation | kfree() before unmapping | `kheap_remove_cluster(...)` |
| 6 | Add spinlock | Wrap all operations | `acquire/release_kspinlock()` |
| 7 | Test | Make && run tests | Verify no crashes/leaks |

---

## Quick Copy-Paste Templates

### Template 1: kheap_add_cluster() in kmalloc()
```c
// After: alloc_page() succeeds for all pages
// Before: update kheapPageAllocBreak

int row = (number_of_pages - 1);
static int cluster_id = 0;
if (row < NUM_KHEAP_ROWS) {
    acquire_kspinlock(&frame_lock);
    kheap_add_cluster(row, cluster_id, new_size, new_va_start);
    cluster_id++;
    release_kspinlock(&frame_lock);
}
```

### Template 2: kheap_remove_cluster() in kfree()
```c
// Find and remove cluster matching va

acquire_kspinlock(&frame_lock);

for (int row = 0; row < NUM_KHEAP_ROWS; row++) {
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        if (elem->virtual_address == va) {
            // Unmap all pages
            for (int j = 0; j < elem->size / PAGE_SIZE; j++) {
                unmap_frame(ptr_page_directory, va + (j * PAGE_SIZE));
            }
            // Remove from list
            LIST_REMOVE(&free_clusters[row], elem);
            free(elem);
            release_kspinlock(&frame_lock);
            return;
        }
    }
}

release_kspinlock(&frame_lock);
```

---

## Decision Trees

### Choosing Row Number
```
Row = ?
    ├─ Based on pages:  row = (pages - 1)
    ├─ Based on size:   row = (size / 1024) - 1
    ├─ Custom:         row = my_custom_calculation()
    └─ MUST BE:        0 <= row < NUM_KHEAP_ROWS
```

### Finding a Cluster
```
Find cluster?
    ├─ By index:  kheap_get_cluster(row, idx)
    ├─ By VA:     Loop & check elem->virtual_address
    ├─ First:     kheap_get_first_cluster(row)
    └─ All in row: LIST_FOREACH(elem, &free_clusters[row])
```

---

## Gotchas & Reminders

⚠️ **Must lock**: Any cluster operation needs spinlock
⚠️ **Must free**: After removing cluster, call free(elem)
⚠️ **Row bounds**: Check `0 <= row < NUM_KHEAP_ROWS`
⚠️ **LIST_FOREACH**: Can't nest - use manual loop inside
⚠️ **Cluster lifetime**: Exists from add → remove
⚠️ **Initialize**: Call kheap_clusters_init() in kheap_init()

---

## Expected Results After Implementation

```
Before: 
  ❌ Can't track allocations
  ❌ Can't find what to free
  ❌ Manual array juggling

After:
  ✅ All allocations tracked automatically
  ✅ Easy to find clusters by VA
  ✅ Clean, organized structure
  ✅ Works with all strategies (FIRSTFIT, BESTFIT, etc)
```

---

## Testing

```bash
# Compile
make clean && make

# Run with heap allocations
# Should see no crashes, no memory leaks

# Debug: Add this to print state
print_all_clusters();  // See all clusters in memory
```

---

## Read These Files (In Order)

1. **THIS FILE** (Cheat sheet - 2 min)
2. `IMPLEMENTATION_CHECKLIST.md` (Checklist - 5 min)
3. `HOW_TO_USE_CLUSTERS.md` (Guide with code - 20 min)
4. `VISUAL_EXAMPLE.md` (Diagrams - 10 min)
5. **Implement** (Follow steps 1-7 above - 30 min)
6. **Test** (Compile & run - 10 min)

**Total time: ~75 minutes** ⏱️

---

## One Last Thing

The array `free_clusters[NUM_KHEAP_ROWS]` is **already defined** in `kheap.h` ✅

You just need to:
1. Add the 8 helper functions to use it
2. Call them at the right times
3. Wrap with spinlocks

That's it! You're ready to code. 💪

