# Solution: Dynamic PA → VA Mapping for Scattered Physical Memory

## The Problem with Fixed Array
```c
uint32 framesArr[32768];  // WRONG!
```

❌ Physical frames are **NOT** contiguous (0 to 32768)
❌ A frame allocated at end of RAM could have frame# = 2,000,000+
❌ This wastes memory and doesn't handle all frames

**Example of scattered allocation:**
```
RAM Start ────────────────────────────────────────────── RAM End
   │                                                        │
   ├─ Frame 100 (PA: 0x64000)     ✓ Allocated
   │
   ├─ Frame 1000 (PA: 0x3E8000)   ✗ Free
   │
   └─ Frame 500000 (PA: 0x7D000000) ✓ Allocated
      (At end of RAM - frame# is HUGE!)
```

---

## Best Solution: Use FOS FrameInfo System ⭐

The FOS system already tracks **every physical frame** with `FrameInfo` structures!

**Key insight:** Don't store PA→VA separately. Instead, when you **allocate memory**, store the VA in the FrameInfo itself (or in a separate tracking structure).

---

## Option A: Track Allocations Locally (Simplest)

Instead of mapping **all possible frames**, only track **allocated clusters**:

```c
/* In kheap.h */
#define MAX_KHEAP_CLUSTERS 1000

struct AllocatedCluster {
    uint32 virtual_address;    // VA of this cluster
    uint32 physical_address;   // PA of this cluster
};

struct AllocatedCluster allocatedClusters[MAX_KHEAP_CLUSTERS];
int numAllocatedClusters = 0;

// Memory used: 1000 × 8 bytes = 8 KB (0.2% of current usage!)
```

**Reverse lookup:**
```c
unsigned int kheap_virtual_address(unsigned int physical_address) {
    for (int i = 0; i < numAllocatedClusters; i++) {
        if (allocatedClusters[i].physical_address == physical_address) {
            return allocatedClusters[i].virtual_address;
        }
    }
    return 0;  // Not found
}
```

**When allocating:**
```c
// After successful allocation:
if (numAllocatedClusters < MAX_KHEAP_CLUSTERS) {
    allocatedClusters[numAllocatedClusters].virtual_address = va;
    allocatedClusters[numAllocatedClusters].physical_address = pa;
    numAllocatedClusters++;
}
```

---

## Option B: Use Hash Table (Faster Lookups)

For faster O(1) lookups with dynamic PA values:

```c
/* Simple hash table */
#define HASH_TABLE_SIZE 1024

struct FrameMapping {
    uint32 physical_address;
    uint32 virtual_address;
    int used;  // 1 = used, 0 = empty
};

struct FrameMapping frameHashTable[HASH_TABLE_SIZE];

// Hash function (simple modulo)
uint32 hash_pa(uint32 pa) {
    return (pa / PAGE_SIZE) % HASH_TABLE_SIZE;
}

// Store PA→VA
void store_frame_mapping(uint32 pa, uint32 va) {
    uint32 idx = hash_pa(pa);
    frameHashTable[idx].physical_address = pa;
    frameHashTable[idx].virtual_address = va;
    frameHashTable[idx].used = 1;
}

// Lookup VA from PA
uint32 kheap_virtual_address(uint32 pa) {
    uint32 idx = hash_pa(pa);
    if (frameHashTable[idx].used && frameHashTable[idx].physical_address == pa) {
        return frameHashTable[idx].virtual_address;
    }
    return 0;
}

// Memory used: 1024 × 12 bytes = 12 KB
```

---

## Option C: Leverage FOS's Existing Page Table System (BEST) 🎯

**Most elegant solution:** Don't store extra mappings at all!

Use the page directory/page table to do the reverse lookup:

```c
unsigned int kheap_virtual_address(unsigned int physical_address) {
    // Physical memory is mapped at KERNEL_BASE (0xF0000000)
    // So: kernel_va = KERNEL_BASE + physical_address
    
    uint32 kernel_mapped_va = KERNEL_BASE + physical_address;
    
    // Now search which kernel heap VA maps to this physical address
    for (uint32 va = kheapPageAllocStart; va < kheapPageAllocBreak; va += PAGE_SIZE) {
        uint32 *page_table = NULL;
        if (get_page_table(ptr_page_directory, va, &page_table) == TABLE_IN_MEMORY) {
            uint32 entry = page_table[PTX(va)];
            if ((entry & PERM_PRESENT) && ((entry & 0xFFFFF000) == (physical_address & 0xFFFFF000))) {
                uint32 offset = physical_address & 0xFFF;
                return va + offset;
            }
        }
    }
    return 0;
}
```

**Advantages:**
- ✅ **0 bytes extra memory** for PA→VA mappings
- ✅ Always accurate (queries page tables directly)
- ✅ Uses FOS system as intended
- ⚠️ Slightly slower (needs to iterate through allocated pages)

---

## Memory Comparison

| Option | Memory | Speed | Complexity | Best For |
|--------|--------|-------|-----------|----------|
| Current | 4 MB | O(1) | Low | ❌ Wasteful |
| Option A | 8 KB | O(n) | Low | ✅ Small allocations |
| Option B | 12 KB | O(1) avg | Medium | Balanced |
| Option C | 0 KB | O(n) | Low | ✅ **FOS-native** |

---

## My Recommendation: Option A (Simple & Effective)

**Why:**
- ✅ 99.8% memory reduction (4MB → 8KB)
- ✅ Very simple to implement
- ✅ Only tracks **allocated** clusters
- ✅ Works with scattered physical addresses
- ✅ Linear search is fast for typical heap size

**Implementation:**

```c
/* In kheap.h */
#define MAX_KHEAP_CLUSTERS 1000

struct AllocatedCluster {
    uint32 virtual_address;
    uint32 physical_address;
};

struct AllocatedCluster allocatedClusters[MAX_KHEAP_CLUSTERS];
int numAllocatedClusters = 0;

/* In kheap.c - when storing allocation */
if (numAllocatedClusters < MAX_KHEAP_CLUSTERS) {
    allocatedClusters[numAllocatedClusters].virtual_address = va;
    allocatedClusters[numAllocatedClusters].physical_address = pa;
    numAllocatedClusters++;
}

/* In kheap.c - reverse lookup */
unsigned int kheap_virtual_address(unsigned int physical_address) {
    for (int i = 0; i < numAllocatedClusters; i++) {
        if (allocatedClusters[i].physical_address == physical_address)
            return allocatedClusters[i].virtual_address;
    }
    return 0;
}

/* When freeing */
void mark_cluster_free(uint32 pa) {
    for (int i = 0; i < numAllocatedClusters; i++) {
        if (allocatedClusters[i].physical_address == pa) {
            // Remove by shifting remaining entries
            for (int j = i; j < numAllocatedClusters - 1; j++) {
                allocatedClusters[j] = allocatedClusters[j + 1];
            }
            numAllocatedClusters--;
            break;
        }
    }
}
```

**Memory calculation:**
```
1000 clusters × (4 bytes VA + 4 bytes PA) = 8 KB
vs original: 4 MB
Savings: 99.8%!
```

