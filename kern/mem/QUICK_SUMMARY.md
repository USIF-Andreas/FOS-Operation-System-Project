# Quick Visual Summary

## The Structure (in kheap.h) ✅

```c
struct KheapCluster {
    int cluster_index;      // ← Use as index!
    LIST_ENTRY(...) prev_next_info;
};

// Tracking arrays (use cluster_index to lookup)
int cluster_size[32766];
uint32 cluster_va[32766];

// Linked lists (one per row)
struct KheapClusterList free_clusters[32766];
```

---

## In Memory (After 3 allocations)

```
╔════════════════════════════════════════════════════════╗
║              LINKED LISTS (By Row)                      ║
╠════════════════════════════════════════════════════════╣
║                                                         ║
║  Row 0 → [idx:6] ↔ [idx:7] → NULL                     ║
║          (4KB each)                                     ║
║                                                         ║
║  Row 1 → [idx:5] → NULL                               ║
║          (8KB)                                          ║
║                                                         ║
║  Row 2 → NULL                                          ║
║                                                         ║
╚════════════════════════════════════════════════════════╝

╔════════════════════════════════════════════════════════╗
║           TRACKING ARRAYS (Indexed)                     ║
╠════════════════════════════════════════════════════════╣
║                                                         ║
║  cluster_size[5] = 8192                               ║
║  cluster_va[5]   = 0xF0000000                         ║
║                                                         ║
║  cluster_size[6] = 4096                               ║
║  cluster_va[6]   = 0xF0002000                         ║
║                                                         ║
║  cluster_size[7] = 4096                               ║
║  cluster_va[7]   = 0xF0003000                         ║
║                                                         ║
╚════════════════════════════════════════════════════════╝
```

---

## The Flow

### kmalloc(8192):
```
Allocate pages → new_va = 0xF0000000, size = 8192
      ↓
Add cluster:
  cluster_size[5] = 8192
  cluster_va[5] = 0xF0000000
  Add [idx:5] to free_clusters[1]
      ↓
Return 0xF0000000
```

### kfree(0xF0000000):
```
Search all rows for cluster_va[] == 0xF0000000
      ↓
Found idx=5 in free_clusters[1]
      ↓
Get size: cluster_size[5] = 8192
      ↓
Unmap: 8192/4096 = 2 pages
      ↓
Remove [idx:5] from free_clusters[1]
      ↓
Clear: cluster_size[5] = 0, cluster_va[5] = 0
```

---

## The 3 Functions You Need

```c
1️⃣  kheap_clusters_init()
    Initialize lists and arrays
    
2️⃣  kheap_add_cluster(row, idx, size, va)
    Store size and VA in arrays
    Add node to linked list
    
3️⃣  kheap_remove_cluster(row, idx)
    Remove from linked list
    Return the node
```

---

## Usage Points

```c
// Initialize (once at boot)
kheap_clusters_init();

// Add (in kmalloc after allocating)
kheap_add_cluster(row, cluster_id, size, va);

// Remove (in kfree when freeing)
struct KheapCluster *elem = kheap_remove_cluster(row, cluster_id);
free(elem);

// Lookup size
int size = cluster_size[elem->cluster_index];

// Lookup VA
uint32 va = cluster_va[elem->cluster_index];
```

---

## That's It! 🎉

Everything is ready. Just add the 3 functions to kheap.c and use them!

