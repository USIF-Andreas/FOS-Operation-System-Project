# Simplified Approach: Visual Explanation

## The Idea: Use Index as Lookup

Instead of storing `size` and `virtual_address` inside the struct, use the `cluster_index` as an index into two separate arrays:

```
BEFORE (Complex):
┌─────────────────────────────────┐
│ struct KheapCluster             │
├─────────────────────────────────┤
│ cluster_index: 5                │
│ size: 8192                      │ ← Duplicated!
│ virtual_address: 0xF0000000     │ ← Duplicated!
│ le_next, le_prev                │
└─────────────────────────────────┘

AFTER (Simple):
┌─────────────────────────────────┐
│ struct KheapCluster             │
├─────────────────────────────────┤
│ cluster_index: 5    ─────┐      │
│ le_next, le_prev    │    │      │
└─────────────────────────────────┘
                      │
        ┌─────────────┴──────────────┐
        ↓                            ↓
    ┌─────────────────────┐  ┌──────────────────────┐
    │ cluster_size[5]     │  │ cluster_va[5]        │
    │ = 8192              │  │ = 0xF0000000         │
    └─────────────────────┘  └──────────────────────┘
```

---

## Flow: kmalloc() → Add Cluster

```
USER: kmalloc(8192)
    ↓
ALLOCATE 2 pages at 0xF0000000
    ↓
CREATE CLUSTER NODE:
    cluster_idx = 5
    ↓
STORE METADATA IN ARRAYS:
    cluster_size[5] = 8192
    cluster_va[5] = 0xF0000000
    ↓
ADD NODE TO LINKED LIST:
    free_clusters[1] → [Node: idx=5] → NULL
    ↓
RETURN 0xF0000000 to user
```

---

## Flow: kfree(0xF0000000) → Remove Cluster

```
USER: kfree(0xF0000000)
    ↓
SEARCH: "Find VA 0xF0000000 in cluster_va[]"
    ↓
FOUND: cluster_va[5] == 0xF0000000
    ↓
RETRIEVE SIZE: cluster_size[5] = 8192
    ↓
UNMAP PAGES:
    for j = 0 to 1:  unmap_frame(..., 0xF0000000 + j*PAGE_SIZE)
    ↓
REMOVE NODE FROM LIST:
    LIST_REMOVE(&free_clusters[1], node_with_idx_5)
    ↓
FREE NODE:
    free(node)
    ↓
CLEAR TRACKING:
    cluster_size[5] = 0
    cluster_va[5] = 0
```

---

## Array Visualization

After 3 allocations:

```
Tracking Arrays:
┌───────────────────────────────────────────┐
│ INDEX │ cluster_size[] │ cluster_va[]     │
├───────┼────────────────┼──────────────────┤
│  0    │     0          │       0          │ (empty)
│  1    │     0          │       0          │ (empty)
│  2    │     0          │       0          │ (empty)
│  3    │     0          │       0          │ (empty)
│  4    │     0          │       0          │ (empty)
│  5    │    8192        │  0xF0000000      │ ← Allocation 1
│  6    │    4096        │  0xF0002000      │ ← Allocation 2
│  7    │    4096        │  0xF0003000      │ ← Allocation 3
│  ... │     ...        │      ...         │
└───────┴────────────────┴──────────────────┘


Linked Lists (free_clusters):
Row 0: → [idx:6] ↔ [idx:7] → NULL
       (2 clusters of 4096 bytes each)

Row 1: → [idx:5] → NULL
       (1 cluster of 8192 bytes = 2 pages)

Row 2: → NULL
       (empty)
```

---

## Code Pattern: kfree Search

```c
// Find cluster by searching VA
for (int row = 0; row < NUM_KHEAP_ROWS; row++) {
    struct KheapCluster *elem;
    LIST_FOREACH(elem, &free_clusters[row]) {
        // Use cluster_index to lookup size and VA
        if (cluster_va[elem->cluster_index] == va) {
            // Found it!
            int idx = elem->cluster_index;
            int size = cluster_size[idx];  // ← Array lookup!
            
            // Unmap pages
            for (int j = 0; j < size / PAGE_SIZE; j++) {
                unmap_frame(..., va + j*PAGE_SIZE);
            }
            
            // Remove and clean up
            LIST_REMOVE(&free_clusters[row], elem);
            free(elem);
            cluster_size[idx] = 0;
            cluster_va[idx] = 0;
            return;
        }
    }
}
```

---

## Benefits of This Approach

| Aspect | Benefit |
|--------|---------|
| **Struct Size** | Only 12 bytes instead of 20+ |
| **Memory Efficient** | Single tracking arrays for all clusters |
| **Simple Access** | `cluster_size[idx]` and `cluster_va[idx]` |
| **Fast Lookup** | O(1) array access vs struct member access |
| **No Duplication** | Data stored once, accessed via index |

---

## Example: Complete Initialization

```c
void kheap_clusters_init(void) {
    // Initialize all linked lists
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

---

## Example: Add Cluster

```c
void kheap_add_cluster(int row, int cluster_idx, int size, uint32 va) {
    if (row >= NUM_KHEAP_ROWS || cluster_idx >= NUM_KHEAP_ROWS) {
        return;  // Bounds check
    }
    
    // 1. Store in tracking arrays
    cluster_size[cluster_idx] = size;
    cluster_va[cluster_idx] = va;
    
    // 2. Create node
    struct KheapCluster *node = malloc(sizeof(struct KheapCluster));
    node->cluster_index = cluster_idx;
    
    // 3. Add to linked list
    LIST_INSERT_HEAD(&free_clusters[row], node);
}
```

---

## Example: Remove Cluster

```c
struct KheapCluster* kheap_remove_cluster(int row, int cluster_idx) {
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

---

## When to Use Each Array

| Situation | Use |
|-----------|-----|
| **Find how many pages to unmap** | `cluster_size[idx] / PAGE_SIZE` |
| **Find where cluster is in memory** | `cluster_va[idx]` |
| **Get cluster from linked list** | `elem->cluster_index` |
| **Iterate clusters in row** | `LIST_FOREACH(elem, &free_clusters[row])` |

---

## Summary

```
✅ Linked lists store: cluster_index (integer)
✅ Tracking arrays store: size and VA (indexed by cluster_index)
✅ Simple: cluster_size[5] and cluster_va[5]
✅ Efficient: No duplication, fast lookup
✅ Clean: Minimal struct, clear separation
```

