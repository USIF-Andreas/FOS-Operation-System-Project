# How to Check if Page Has Blocks After free_block()

## Scenario
You have a virtual address of a block and want to:
1. Free it using `free_block(va)`
2. Check if the page still has allocated blocks or is completely free

---

## Solution: Step-by-Step

### **Step 1: Get Page Information from Block's VA**

```c
uint32 block_va = 0xF6001234;  // Your block address

// Calculate which page this block belongs to
uint32 pageIndex = (block_va - dynAllocStart) / PAGE_SIZE;
struct PageInfoElement* page = &pageBlockInfoArr[pageIndex];

printf("Block VA: 0x%x\n", block_va);
printf("Page Index: %d\n", pageIndex);
printf("Page Info: block_size=%d, num_of_free_blocks=%d\n", 
       page->block_size, page->num_of_free_blocks);
```

---

### **Step 2: Free the Block**

```c
free_block((void*)block_va);  // Free it
```

---

### **Step 3: Check Page Status After free_block()**

#### **Option A: Check if Page Has Any Allocated Blocks**

```c
// After free_block(block_va):
struct PageInfoElement* page = &pageBlockInfoArr[pageIndex];

uint32 total_blocks = PAGE_SIZE / page->block_size;
uint32 allocated_blocks = total_blocks - page->num_of_free_blocks;

if (page->block_size == 0) {
    printf("✓ Page is COMPLETELY FREE (unmapped)\n");
    printf("  Page returned to kernel\n");
} else if (allocated_blocks == 0) {
    printf("✗ ERROR: Page should be unmapped but block_size != 0\n");
} else {
    printf("✓ Page still has ALLOCATED BLOCKS\n");
    printf("  Total blocks: %d\n", total_blocks);
    printf("  Free blocks: %d\n", page->num_of_free_blocks);
    printf("  Allocated blocks: %d\n", allocated_blocks);
}
```

---

#### **Option B: Simpler Check - Is Page Completely Free?**

```c
// Check if page was completely freed (and unmapped)
if (page->block_size == 0) {
    printf("✓ Page UNMAPPED (completely free)\n");
} else {
    // Page still in use, check how many blocks left
    uint32 total = PAGE_SIZE / page->block_size;
    uint32 free = page->num_of_free_blocks;
    uint32 allocated = total - free;
    
    if (allocated > 0) {
        printf("✓ Page still has %d ALLOCATED blocks\n", allocated);
    }
}
```

---

## Complete Example Function

### **Function to Check Page After Freeing**

```c
void check_page_after_free(uint32 block_va) {
    // Get page information
    uint32 pageIndex = (block_va - dynAllocStart) / PAGE_SIZE;
    struct PageInfoElement* page = &pageBlockInfoArr[pageIndex];
    
    printf("\n=== CHECK PAGE AFTER FREE ===\n");
    printf("Block VA: 0x%x\n", block_va);
    printf("Page Index: %d\n", pageIndex);
    
    // Case 1: Page is unmapped
    if (page->block_size == 0) {
        printf("Status: PAGE COMPLETELY FREED ✓\n");
        printf("  - block_size = 0 (unmapped)\n");
        printf("  - num_of_free_blocks = 0\n");
        printf("  - Page returned to kernel\n");
        
        // Verify with page table
        uint32 page_va = to_page_va(page);
        uint32 *page_table = NULL;
        if (get_page_table(ptr_page_directory, page_va, &page_table) == TABLE_IN_MEMORY) {
            uint32 pte = page_table[PTX(page_va)];
            if ((pte & PERM_PRESENT) == 0) {
                printf("  - PTE.PRESENT = 0 (confirmed unmapped)\n");
            }
        }
        return;
    }
    
    // Case 2: Page still in use
    uint32 total_blocks = PAGE_SIZE / page->block_size;
    uint32 free_blocks = page->num_of_free_blocks;
    uint32 allocated_blocks = total_blocks - free_blocks;
    
    printf("Status: PAGE STILL IN USE\n");
    printf("  - block_size: %d bytes\n", page->block_size);
    printf("  - Total blocks per page: %d\n", total_blocks);
    printf("  - Free blocks: %d\n", free_blocks);
    printf("  - Allocated blocks: %d\n", allocated_blocks);
    
    if (allocated_blocks == 0 && page->block_size != 0) {
        printf("  - WARNING: All blocks free but page not unmapped yet!\n");
    }
}
```

---

## Usage Example

```c
// Example 1: Free a block from a page with multiple blocks
uint32 block1_va = 0xF6001000;
uint32 block2_va = 0xF6001040;  // Same page, different block

printf("Before freeing:\n");
check_page_after_free(block1_va);

printf("\n--- Freeing block1 ---\n");
free_block((void*)block1_va);

printf("\nAfter freeing block1:\n");
check_page_after_free(block1_va);

printf("\n--- Freeing block2 ---\n");
free_block((void*)block2_va);

printf("\nAfter freeing block2 (last block):\n");
check_page_after_free(block1_va);  // Will show "PAGE COMPLETELY FREED"
```

---

## Quick Reference Table

### **What to Check**

| What to Check | How to Check | Result |
|---------------|--------------|--------|
| **Is page unmapped?** | `page->block_size == 0` | YES = unmapped, NO = still in use |
| **How many blocks are allocated?** | `(PAGE_SIZE / page->block_size) - page->num_of_free_blocks` | Number > 0 = blocks exist |
| **How many blocks are free?** | `page->num_of_free_blocks` | Number = total means page is full |
| **Is page in free list?** | Check `freePagesList` | YES = unmapped, NO = in use |
| **Is page in memory?** | Check `PTE.PRESENT` bit | 1 = in memory, 0 = unmapped |

---

## Common Scenarios

### **Scenario 1: Free Last Block in Page**

```c
// Page has 128 blocks (block_size=32)
// Currently: 1 allocated, 127 free

free_block(last_allocated_block_va);

// After:
struct PageInfoElement* page = &pageBlockInfoArr[pageIndex];

if (page->block_size == 0) {
    printf("✓ Page unmapped! Returned to kernel.\n");
} else {
    printf("✗ Page should be unmapped but isn't!\n");
}
```

**Expected Result:** `page->block_size == 0` (page unmapped) ✓

---

### **Scenario 2: Free One Block, Others Still Allocated**

```c
// Page has 128 blocks (block_size=32)
// Currently: 5 allocated, 123 free

free_block(one_block_va);

// After:
uint32 total = PAGE_SIZE / page->block_size;
uint32 allocated = total - page->num_of_free_blocks;

printf("Allocated blocks remaining: %d\n", allocated);
// Output: Allocated blocks remaining: 4

if (page->block_size != 0) {
    printf("✓ Page still in use (not unmapped)\n");
}
```

**Expected Result:** Page still has 4 allocated blocks ✓

---

### **Scenario 3: Check Multiple Allocations in Same Page**

```c
// Create 3 blocks in same page
uint32 block_a = (uint32)alloc_block(32);
uint32 block_b = (uint32)alloc_block(32);
uint32 block_c = (uint32)alloc_block(32);

// Get page
uint32 pageIndex = (block_a - dynAllocStart) / PAGE_SIZE;
struct PageInfoElement* page = &pageBlockInfoArr[pageIndex];

printf("Page status after allocation:\n");
printf("  block_size: %d\n", page->block_size);
uint32 total = PAGE_SIZE / page->block_size;
printf("  Allocated blocks: %d\n", total - page->num_of_free_blocks);

// Free blocks one by one
free_block((void*)block_a);
printf("\nAfter freeing block_a:\n");
printf("  Allocated: %d, block_size: %d\n", 
       total - page->num_of_free_blocks, page->block_size);

free_block((void*)block_b);
printf("\nAfter freeing block_b:\n");
printf("  Allocated: %d, block_size: %d\n", 
       total - page->num_of_free_blocks, page->block_size);

free_block((void*)block_c);
printf("\nAfter freeing block_c (last one):\n");
printf("  Allocated: %d, block_size: %d (0 = unmapped)\n", 
       (page->block_size == 0) ? 0 : total - page->num_of_free_blocks, 
       page->block_size);
```

---

## Key Points

✅ **After `free_block(va)`:**
- If all blocks in page are freed → `page->block_size = 0` (UNMAPPED)
- If some blocks still allocated → `page->block_size > 0` (STILL IN USE)

✅ **To check if blocks exist:**
```c
uint32 allocated = (PAGE_SIZE / page->block_size) - page->num_of_free_blocks;
if (allocated > 0) {
    // Page still has allocated blocks
}
```

✅ **To check if page is completely free:**
```c
if (page->block_size == 0) {
    // Page is unmapped and returned to kernel
}
```

