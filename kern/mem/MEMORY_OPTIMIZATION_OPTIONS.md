# Memory Optimization Options for PA → VA Mapping

## Current Problem
```c
uint32 framesArr[1000000];  // 4 MB wasted!
```

---

## Option 1: Use Only Needed Frames (RECOMMENDED) ⭐

**Concept:** Physical frames = Physical Memory / PAGE_SIZE
- For 128MB RAM: 128MB / 4KB = 32,768 frames max
- For 64MB RAM: 64MB / 4KB = 16,384 frames max
- For 32MB RAM: 32MB / 4KB = 8,192 frames max

**Implementation:**
```c
#define MAX_FRAMES 32768  // Adjust based on your RAM
uint32 framesArr[MAX_FRAMES];

// Memory used: 32,768 × 4 bytes = 128 KB (vs 4 MB)
// Savings: 97% reduction!
```

**Mapping:**
```
Frame Number = Physical Address >> 12
Example: PA = 0x1000000 → Frame = 0x1000000 >> 12 = 4096
         framesArr[4096] = Virtual Address
```

---

## Option 2: Use uint16 Instead of uint32

**Concept:** Many virtual addresses fit in 16 bits if:
- Heap starts at fixed address (e.g., 0xF6000000)
- Only store offset from heap start

**Implementation:**
```c
#define MAX_FRAMES 32768
#define HEAP_VA_BASE 0xF6000000

uint16 framesArr_offset[MAX_FRAMES];  // 16-bit offsets only

// When storing:
uint32 va = 0xF6001000;
uint16 offset = (va - HEAP_VA_BASE) >> 12;  // Convert to page offset
framesArr_offset[frame_num] = offset;

// When retrieving:
uint32 va = HEAP_VA_BASE + (framesArr_offset[frame_num] << 12);
```

**Memory used:** 32,768 × 2 bytes = 64 KB (97.5% reduction!)

---

## Option 3: Sparse Hash Table

**Concept:** Only store mappings for allocated frames (not all possible frames)

**Implementation:**
```c
#define MAX_ALLOCATED_CLUSTERS 1000

struct FrameMapping {
    uint32 frame_num;
    uint32 virtual_address;
};

struct FrameMapping framesMap[MAX_ALLOCATED_CLUSTERS];
int num_mappings = 0;

// When storing:
void store_mapping(uint32 frame_num, uint32 va) {
    if (num_mappings < MAX_ALLOCATED_CLUSTERS) {
        framesMap[num_mappings].frame_num = frame_num;
        framesMap[num_mappings].virtual_address = va;
        num_mappings++;
    }
}

// When retrieving:
uint32 get_va_from_pa(uint32 pa) {
    uint32 frame_num = pa >> 12;
    for (int i = 0; i < num_mappings; i++) {
        if (framesMap[i].frame_num == frame_num)
            return framesMap[i].virtual_address;
    }
    return 0;
}
```

**Memory used:** Variable (8 × 1000 = 8 KB for 1000 entries)
**Tradeoff:** Slower lookup (O(n) instead of O(1))

---

## Option 4: Use FOS's Existing System (BEST) 🎯

**Concept:** Don't store PA→VA mapping at all! Use FOS's page table system:

```c
// Instead of storing framesArr, use:
unsigned int kheap_virtual_address(unsigned int physical_address) {
    struct FrameInfo *ptr_frame = to_frame_info(physical_address);
    if (ptr_frame == NULL) return 0;
    
    uint32 frame_number = physical_address >> 12;
    uint32 offset = physical_address & 0xFFF;
    
    // Search through mapped pages to find this frame
    for (uint32 va = kheapPageAllocStart; va < kheapPageAllocBreak; va += PAGE_SIZE) {
        uint32 *page_table = NULL;
        struct FrameInfo *frame_at_va = get_frame_info(ptr_page_directory, va, &page_table);
        
        if (frame_at_va && to_frame_number(frame_at_va) == frame_number)
            return va + offset;
    }
    return 0;
}
```

**Memory used:** 0 KB for framesArr!
**Tradeoff:** Slightly slower (searches page table) but CPU caches help

---

## Comparison Table

| Option | Memory Used | Speed | Implementation | Savings |
|--------|------------|-------|-----------------|---------|
| Current | 4 MB | O(1) | ❌ Wasteful | 0% |
| Option 1 | 128 KB | O(1) | ✅ Easy | 97% |
| Option 2 | 64 KB | O(1) | ✅ Easy | 98.4% |
| Option 3 | 8 KB | O(n) | 🟡 Medium | 99.8% |
| Option 4 | 0 KB | O(n) | ✅ Standard | 100% |

---

## My Recommendation: Option 1 (Balanced)

**Why:**
- ✅ 97% memory reduction (4MB → 128KB)
- ✅ Same fast O(1) lookup
- ✅ Simple to implement
- ✅ Maximum frame count matches real hardware

**Code:**
```c
// In kheap.h
#define MAX_KHEAP_FRAMES 32768  // Your actual max frames
uint32 framesArr[MAX_KHEAP_FRAMES];

// In kheap_init()
for (int i = 0; i < MAX_KHEAP_FRAMES; i++) {
    framesArr[i] = 0;  // 0 = not mapped
}

// When storing PA→VA mapping
uint32 ph = kheap_physical_address(va);
uint32 frame_num = ph >> 12;
if (frame_num < MAX_KHEAP_FRAMES) {
    framesArr[frame_num] = va;
}

// When retrieving VA from PA
uint32 kheap_virtual_address(uint32 pa) {
    uint32 frame_num = pa >> 12;
    if (frame_num >= MAX_KHEAP_FRAMES) return 0;
    return framesArr[frame_num];
}
```

---

## How to Find Your MAX_KHEAP_FRAMES

Check your system's physical memory:
```
Physical RAM Size / PAGE_SIZE = MAX_FRAMES

Examples:
- 128 MB RAM → 128 × 1024 × 1024 / 4096 = 32,768 ✓
-  64 MB RAM →  64 × 1024 × 1024 / 4096 = 16,384
-  32 MB RAM →  32 × 1024 × 1024 / 4096 =  8,192
```

**For FOS/Bochs typically:** Use 32,768 (safe upper limit)

---

## Quick Calculation Tool

```
Current: 1,000,000 entries × 4 bytes = 4,000,000 bytes = 4 MB

Option 1:  32,768 entries × 4 bytes =    131,072 bytes = 128 KB ✓
Option 2:  32,768 entries × 2 bytes =     65,536 bytes = 64 KB
Option 4:       0 entries × 4 bytes =          0 bytes = 0 KB (but slower)
```

