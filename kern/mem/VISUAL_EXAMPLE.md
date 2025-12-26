# Visual: How free_clusters Works in Practice

## The Journey of a Memory Allocation

### 1. INITIALIZATION (Boot Time)

```
kheap_init() is called
    ↓
kheap_clusters_init()
    ↓
for (int i = 0; i < 32766; i++)
    LIST_INIT(&free_clusters[i]);
    ↓
RESULT:
    free_clusters[0] → NULL
    free_clusters[1] → NULL
    free_clusters[2] → NULL
    ...
    free_clusters[32765] → NULL
    
(All 32766 rows are empty, ready to store clusters)
```

---

### 2. ALLOCATION (kmalloc called)

```
User calls: kmalloc(8192)
    ↓
size > DYN_ALLOC_MAX_BLOCK_SIZE
    ↓
Allocate 2 pages (8 KB = 2×4KB)
- Page 1: allocated at 0xF0000000
- Page 2: allocated at 0xF0001000
    ↓
UPDATE: kheapPageAllocBreak += 8192
    ↓
TRACK IT: kheap_add_cluster(row, cluster_idx, 8192, 0xF0000000)
    ↓
RESULT:
    free_clusters[1] → [KheapCluster]
                       ├─ cluster_index: 0
                       ├─ size: 8192
                       ├─ virtual_address: 0xF0000000
                       └─ next: NULL
                       
(Row 1 = 2 pages, so row = pages-1 = 2-1 = 1)

Return: 0xF0000000
```

---

### 3. ANOTHER ALLOCATION

```
User calls: kmalloc(4096)
    ↓
Allocate 1 page (4 KB = 1×4KB)
- Page: allocated at 0xF0002000
    ↓
kheap_add_cluster(0, 1, 4096, 0xF0002000)
    ↓
RESULT:
    free_clusters[0] → [KheapCluster]
                       ├─ cluster_index: 1
                       ├─ size: 4096
                       ├─ virtual_address: 0xF0002000
                       └─ next: NULL
    
    free_clusters[1] → [KheapCluster]
                       ├─ cluster_index: 0
                       ├─ size: 8192
                       ├─ virtual_address: 0xF0000000
                       └─ next: NULL

Return: 0xF0002000
```

---

### 4. YET ANOTHER ALLOCATION (Same Size)

```
User calls: kmalloc(4096) again
    ↓
Allocate 1 page (4 KB)
- Page: allocated at 0xF0003000
    ↓
kheap_add_cluster(0, 2, 4096, 0xF0003000)
    ↓
RESULT:
    free_clusters[0] → [Cluster 2] ↔ [Cluster 1] → NULL
                       [VA:F0003000] [VA:F0002000]
    
    free_clusters[1] → [Cluster 0] → NULL
                       [VA:F0000000]

Return: 0xF0003000
```

---

### 5. DEALLOCATION (kfree called)

```
User calls: kfree(0xF0002000)
    ↓
va = 0xF0002000 is in page allocator range
    ↓
Search all rows for this VA
    ↓
Found in free_clusters[0]:
    [Cluster 2] ↔ [Cluster 1] ← MATCH! ↔ NULL
    
    ↓
Unmap pages:
    unmap_frame(ptr_page_directory, 0xF0002000)
    ↓
Remove from list:
    LIST_REMOVE(&free_clusters[0], elem)
    free(elem)
    ↓
RESULT:
    free_clusters[0] → [Cluster 2] → NULL
                       [VA:F0003000]
    
    free_clusters[1] → [Cluster 0] → NULL
                       [VA:F0000000]
```

---

### 6. ANOTHER DEALLOCATION

```
User calls: kfree(0xF0000000)
    ↓
Found in free_clusters[1]:
    [Cluster 0] → NULL
    [VA:F0000000] ← MATCH!
    ↓
Unmap 2 pages:
    unmap_frame(ptr_page_directory, 0xF0000000)
    unmap_frame(ptr_page_directory, 0xF0001000)
    ↓
Remove from list
    ↓
RESULT:
    free_clusters[0] → [Cluster 2] → NULL
                       [VA:F0003000]
    
    free_clusters[1] → NULL (empty now!)
    
    free_clusters[2] → NULL
    ...
```

---

## Key Points Visualized

### ROW vs COLUMN

```
        COLUMNS (Clusters in each row)
             ↓
ROW 0 → [C1] ↔ [C2] ↔ [C3] → NULL
ROW 1 → [C4] → NULL
ROW 2 → NULL
ROW 3 → [C5] ↔ [C6] → NULL
...
ROW N → [C7] → NULL

Each [Cn] is:
    ┌─────────────────┐
    │ cluster_index   │ (C1, C2, etc)
    ├─────────────────┤
    │ size            │ (4096, 8192, etc)
    ├─────────────────┤
    │ virtual_address │ (0xF0000000, etc)
    ├─────────────────┤
    │ le_next/le_prev │ (links to next/prev)
    └─────────────────┘
```

---

## Code Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    KERNEL HEAP LIFECYCLE                   │
└─────────────────────────────────────────────────────────────┘

    ┌──────────────────┐
    │   Boot (Early)   │
    │  kheap_init()    │
    └────────┬─────────┘
             │
             ↓
    ┌────────────────────────────┐
    │ kheap_clusters_init()      │
    │ Initialize 32766 empty     │
    │ linked lists               │
    └────────┬───────────────────┘
             │
             ├─────────────────────────────┐
             │                             │
             ↓                             ↓
    ┌────────────────┐        ┌────────────────┐
    │   kmalloc()    │        │    kfree()     │
    │   ALLOCATE     │        │   DEALLOCATE   │
    └────────┬───────┘        └────────┬───────┘
             │                         │
             ↓                         ↓
    ┌────────────────────┐   ┌────────────────────┐
    │ Allocate pages     │   │ Find cluster       │
    │ Allocate VA        │   │ by virtual_address │
    └────────┬───────────┘   └────────┬───────────┘
             │                         │
             ↓                         ↓
    ┌────────────────────┐   ┌────────────────────┐
    │ kheap_add_cluster()│   │ LIST_REMOVE()      │
    │ INSERT into array  │   │ Remove from list   │
    └────────┬───────────┘   └────────┬───────────┘
             │                         │
             ↓                         ↓
    ┌────────────────────┐   ┌────────────────────┐
    │ Return VA to user  │   │ Unmap pages        │
    │                    │   │ Free cluster node  │
    └────────────────────┘   └────────────────────┘
```

---

## Time Complexity

```
Operation                    | Time
─────────────────────────────┼──────────────────
Initialize all rows          | O(NUM_KHEAP_ROWS)
Add cluster to row           | O(1)
Remove cluster from row      | O(row_size)
Find cluster by index        | O(row_size)
Get row size                 | O(1)
Check if row empty           | O(1)
Traverse all clusters in row | O(row_size)
```

---

## Memory Usage Example

After allocating:
- 3 clusters of 4 KB (1 page each)
- 2 clusters of 8 KB (2 pages each)

```
Total Memory:
    Base array: 32766 × 8 bytes (pointers) = 262,128 bytes
    + Cluster nodes: 5 × 20 bytes = 100 bytes
    ≈ 262 KB allocated

Allocated pages (returned to user):
    3 × 4 KB + 2 × 8 KB = 28 KB
    
Total Overhead:
    Array + cluster nodes ≈ 262 KB
    (One-time cost for managing heap)
```

---

## Locking Strategy

```
┌─────────────────────────────────────────┐
│         Using free_clusters Array       │
├─────────────────────────────────────────┤
│                                         │
│  acquire_kspinlock(&frame_lock)        │ ← Lock
│      ↓                                   │
│  kheap_add_cluster(...)                │
│  kheap_remove_cluster(...)             │
│  LIST_FOREACH(...)                     │
│      ↓                                   │
│  release_kspinlock(&frame_lock)        │ ← Unlock
│                                         │
└─────────────────────────────────────────┘

IMPORTANT: All operations on free_clusters
must be protected by frame_lock spinlock!
```

