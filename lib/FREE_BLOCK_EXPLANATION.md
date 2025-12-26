# Detailed Explanation: `free_block()` Function

## Overview
This function **frees a block of memory** that was previously allocated by `alloc_block()`. It handles:
1. Returning the block to the free list
2. Checking if an entire page is now free
3. Unmapping the page if it becomes completely free

---

## Function Breakdown

### **Part 1: Extract Page Information**
```c
uint32 va_num = (uint32)va;
uint32 pageIndex = (va_num - dynAllocStart) / PAGE_SIZE;
struct PageInfoElement* page = &pageBlockInfoArr[pageIndex];
uint32 block_size = page->block_size;
uint32 listIndex = log2(block_size) - LOG2_MIN_SIZE;
```

**Explanation:**

| Variable | Purpose | Example |
|----------|---------|---------|
| `va_num` | Virtual address as uint32 | 0xF6001234 |
| `pageIndex` | Which page in DA is this? | (0xF6001234 - 0xF6000000) / 4096 = 0 |
| `page` | Pointer to PageInfoElement for this page | &pageBlockInfoArr[0] |
| `block_size` | Size of blocks in this page | 32, 64, 128, ... 2048 bytes |
| `listIndex` | Index into freeBlockLists array | log2(32) - 3 = 5 - 3 = 2 |

**Example:**
```
VA = 0xF6001500
dynAllocStart = 0xF6000000
PAGE_SIZE = 4096

pageIndex = (0xF6001500 - 0xF6000000) / 4096
          = 0x1500 / 4096
          = 5376 / 4096
          = 1  (second page)

page = &pageBlockInfoArr[1]  ← Points to info about 2nd page
```

---

### **Part 2: Add Block Back to Free List**
```c
LIST_INSERT_HEAD(&freeBlockLists[listIndex], (struct BlockElement*)va);
page->num_of_free_blocks++;
```

**What happens:**
1. **Insert block into free list** - Place it at the head of the linked list
2. **Increment counter** - Track how many free blocks are in this page

**Example:**
```
Before free_block():
    Page info: block_size = 32, num_of_free_blocks = 3 (out of 128 blocks)
    Free blocks list[2]: [block_A] → [block_B] → [block_C]

After LIST_INSERT_HEAD():
    Page info: block_size = 32, num_of_free_blocks = 4
    Free blocks list[2]: [block_D (newly freed)] → [block_A] → [block_B] → [block_C]
```

---

### **Part 3: Check if Page is Completely Free**
```c
if (page->num_of_free_blocks == PAGE_SIZE / block_size)
{
    // All blocks in this page are now free!
    // Time to unmap the page
}
```

**Explanation:**

When is a page **completely free**?

```
Formula: num_of_free_blocks == PAGE_SIZE / block_size

Example:
    Page size = 4096 bytes
    Block size = 32 bytes
    Total blocks per page = 4096 / 32 = 128

    If num_of_free_blocks == 128, then ALL blocks are free ✓
    If num_of_free_blocks == 127, then 1 block is still allocated ✗
```

---

### **Part 4: Remove All Blocks from Free List**
```c
struct BlockElement *blk = LIST_FIRST(&freeBlockLists[listIndex]);
while (blk != NULL)
{
    struct BlockElement *next = LIST_NEXT(blk);
    LIST_REMOVE(&freeBlockLists[listIndex], blk);
    blk = next;
}
```

**What happens:**
- Iterates through all blocks in the free list
- Removes each one from the linked list
- Prepares to return the entire page to the kernel

**Visualization:**
```
Before removal:
    Free list[2]: [B1] ↔ [B2] ↔ [B3] ↔ ... ↔ [B128]

After loop iteration 1:
    Free list[2]: [B2] ↔ [B3] ↔ ... ↔ [B128]
    (B1 removed)

After loop iteration 2:
    Free list[2]: [B3] ↔ ... ↔ [B128]
    (B2 removed)

After all iterations:
    Free list[2]: (empty)
```

---

### **Part 5: Unmap the Page from Kernel**
```c
uint32 page_va = to_page_va(page);
return_page((void*)page_va);
page->block_size = 0;
page->num_of_free_blocks = 0;
LIST_INSERT_HEAD(&freePagesList, page);
```

**What happens:**

1. **Get page's virtual address:**
   ```c
   page_va = to_page_va(page);
   // Converts PageInfoElement* back to its VA
   ```

2. **Return page to kernel:**
   ```c
   return_page((void*)page_va);
   // Unmaps page from memory (deallocate physical frame)
   // After this, the page is no longer accessible!
   ```

3. **Reset page tracking:**
   ```c
   page->block_size = 0;           // No more blocks in this page
   page->num_of_free_blocks = 0;   // No blocks to count
   ```

4. **Add page to free pages list:**
   ```c
   LIST_INSERT_HEAD(&freePagesList, page);
   // Page is ready to be reused for new allocations
   ```

---

## How to Know If a Page is Unmapped or Not

### **Method 1: Check `page->block_size`**

```c
struct PageInfoElement* page = &pageBlockInfoArr[pageIndex];

if (page->block_size == 0) {
    printf("Page is UNMAPPED (free)\n");
} else {
    printf("Page is MAPPED (in use), block_size = %d\n", page->block_size);
}
```

**Explanation:**
- `block_size = 0` → Page has been returned to kernel, NOT mapped anymore
- `block_size > 0` → Page is allocated and being used

---

### **Method 2: Check if in Free Pages List**

```c
// Check if page is in the free pages list
struct PageInfoElement* page = LIST_FIRST(&freePagesList);
while (page != NULL) {
    if (page == &pageBlockInfoArr[pageIndex]) {
        printf("Page IS in free list (UNMAPPED)\n");
        break;
    }
    page = LIST_NEXT(page);
}

if (page == NULL) {
    printf("Page is NOT in free list (MAPPED)\n");
}
```

---

### **Method 3: Use `get_page_table()` and Check PTE**

```c
// Get the page from physical memory system
uint32 *page_table = NULL;
int result = get_page_table(ptr_page_directory, page_va, &page_table);

if (result == TABLE_NOT_EXIST || page_table == NULL) {
    printf("Page table doesn't exist (UNMAPPED)\n");
    return;
}

uint32 pte = page_table[PTX(page_va)];

if ((pte & PERM_PRESENT) == 0) {
    printf("Page is NOT PRESENT in memory (UNMAPPED) ✓\n");
} else {
    printf("Page IS PRESENT in memory (MAPPED)\n");
}
```

**How it works:**
- Every page has an entry in the page table (PTE)
- `PERM_PRESENT` bit = 1 means page is loaded in RAM
- `PERM_PRESENT` bit = 0 means page has been unmapped/freed

---

## Complete State Tracking

### **Page Lifecycle**

```
STATE 1: ALLOCATED & IN USE
├─ block_size = 32 (or 64, 128, ...)
├─ num_of_free_blocks = 0 (or < total)
├─ Blocks list: Contains allocated blocks
├─ Free list: Contains free blocks
└─ PTE.PRESENT = 1

        ↓ (All blocks freed)

STATE 2: COMPLETELY FREE (Before Unmapping)
├─ block_size = 32 (still set)
├─ num_of_free_blocks = 128 (ALL blocks free)
├─ Blocks list: Contains all blocks
├─ Free list: About to be emptied
└─ PTE.PRESENT = 1

        ↓ (return_page() called)

STATE 3: UNMAPPED & FREE (After return_page())
├─ block_size = 0 ← RESET!
├─ num_of_free_blocks = 0 ← RESET!
├─ Blocks list: Empty
├─ Free pages list: Contains this page
└─ PTE.PRESENT = 0 ← UNMAPPED!
```

---

## Practical Example

### **Scenario: Free the Last Block in a Page**

```
Initial state:
    Page 5 info:
        - block_size = 32
        - num_of_free_blocks = 127 (out of 128)
        - one block is still allocated at VA = 0xF6015000

Step 1: Call free_block(0xF6015000)
    pageIndex = (0xF6015000 - 0xF6000000) / 4096 = 5
    page = &pageBlockInfoArr[5]
    block_size = 32
    listIndex = 2

Step 2: Add block to free list
    LIST_INSERT_HEAD(&freeBlockLists[2], (0xF6015000))
    num_of_free_blocks++ → 128

Step 3: Check if page is fully free
    if (128 == 4096/32)  ✓ TRUE
    {
        // Page is completely free!
        // Remove all 128 blocks from free list
        // Unmap the page
        // Reset page info
    }

Step 4: After return_page()
    page->block_size = 0
    page->num_of_free_blocks = 0
    Page moved to freePagesList
    
    ✓ Page is now UNMAPPED!
    ✓ PTE.PRESENT = 0
```

### **How to Verify Page is Unmapped**

```c
uint32 page_va = 0xF6014000;  // Assume this is page 5
uint32 *page_table = NULL;

int result = get_page_table(ptr_page_directory, page_va, &page_table);
if (result == TABLE_IN_MEMORY && page_table != NULL) {
    uint32 pte = page_table[PTX(page_va)];
    
    if ((pte & PERM_PRESENT) == 0) {
        printf("✓ Page at 0x%x is UNMAPPED\n", page_va);
        printf("  PTE = 0x%x (PRESENT bit = 0)\n", pte);
    }
}

// Also check PageInfo
struct PageInfoElement* page = &pageBlockInfoArr[5];
if (page->block_size == 0) {
    printf("✓ PageInfoElement confirms: block_size = 0\n");
}
```

---

## Summary

| Check | Unmapped | Mapped |
|-------|----------|--------|
| `page->block_size` | = 0 | > 0 |
| `page->num_of_free_blocks` | = 0 | > 0 |
| In `freePagesList` | YES ✓ | NO ✗ |
| PTE.PRESENT bit | = 0 | = 1 |
| Can access memory | NO ✗ | YES ✓ |

**Best way to check:** 
```c
// Check PageInfoElement
if (page->block_size == 0) {
    printf("Page is unmapped\n");
}

// Double-check with PTE
uint32 pte = page_table[PTX(page_va)];
if ((pte & PERM_PRESENT) == 0) {
    printf("Confirmed: Page not present in memory\n");
}
```

