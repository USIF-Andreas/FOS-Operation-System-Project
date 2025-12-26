# Detailed Explanation: `pa_to_va_scan()` Function

## Overview
This function performs a **reverse lookup** in the page tables to find the **virtual address** that maps to a given **physical address**.

It scans through the entire page directory and page tables to find which VA points to the given PA.

---

## Function Signature
```c
uint32 pa_to_va_scan(uint32 pa, uint32 *page_directory) {
    // pa: Physical address to find
    // page_directory: Pointer to the page directory (base of page tables)
    // Returns: Virtual address that maps to PA, or 0 if not found
}
```

---

## Step-by-Step Breakdown

### **Step 1: Extract Page-Aligned PA and Offset**
```c
uint32 pa_aligned = pa & ~0xFFF;     // Remove last 12 bits (offset)
uint32 offset = pa & 0xFFF;           // Keep only last 12 bits
```

**Explanation:**
- `0xFFF` = 0000_1111_1111_1111 (binary) = 4095 (last 12 bits)
- `~0xFFF` = 1111_0000_0000_0000 (bitwise NOT)
- `pa & ~0xFFF` = Remove offset, keep only page-aligned part
- `pa & 0xFFF` = Keep only offset

**Example:**
```
PA = 0x12345678
  = 0001_0010_0011_0100_0101_0110_0111_1000

pa_aligned = PA & ~0xFFF
           = 0001_0010_0011_0100_0101_0110_0000_0000
           = 0x12345000

offset = PA & 0xFFF
       = 0000_0000_0000_0000_0000_0000_0111_1000
       = 0x678
```

---

### **Step 2: Iterate Through All Page Directory Entries (PDEs)**
```c
for (uint32 pdx = 0; pdx < 1024; ++pdx) {
    uint32 pde = page_directory[pdx];
    if (!(pde & PERM_PRESENT)) continue;  // Skip if page table not present
```

**Explanation:**
- **pdx** = Page Directory indeX (0 to 1023)
- **pde** = Page Directory Entry
- **1024** = Number of entries in page directory
- `PERM_PRESENT` = Bit flag indicating if page table is allocated/present
- `!(pde & PERM_PRESENT)` = If bit is NOT set, skip this entry

**Memory Layout:**
```
Page Directory has 1024 entries
Each entry points to a page table (or a 4MB page in PSE mode)

pdx = 0   → page_directory[0]  → points to first page table
pdx = 1   → page_directory[1]  → points to second page table
...
pdx = 1023→ page_directory[1023]→ points to last page table
```

---

### **Step 3: Check for 4MB Pages (PSE - Page Size Extension)**
```c
if (pde & PDE_PS_BIT) {
    uint32 base4 = pde & 0xFFC00000;
    if ((pa_aligned & 0xFFC00000) == base4) {
        return (pdx << 22) | (pa & 0x3FFFFF);
    }
    continue;
}
```

**Explanation:**
- **PDE_PS_BIT** = Page Size Extension bit (bit 7 in PDE)
- When set: This PDE points directly to a **4MB page** (not a page table)
- When not set: This PDE points to a **page table** (normal 4KB pages)

**Case: 4MB Page Found**
```
base4 = pde & 0xFFC00000        // Extract 4MB page base address
                                 // Keeps bits [31:22]

If this 4MB page contains our PA:
    Calculate VA:
    VA = (pdx << 22) | (pa & 0x3FFFFF)
    
    Where:
    - (pdx << 22)    = Directory index shifted to bits [31:22]
    - (pa & 0x3FFFFF)= Offset within 4MB page (bits [21:0])
    
Example:
    pdx = 2, pa_offset = 0x12345
    VA = (2 << 22) | 0x12345
       = 0x0800_0000 | 0x12345
       = 0x0801_2345
```

---

### **Step 4: Normal Case - Get Page Table from PDE**
```c
uint32 pt_phys = pde & 0xFFFFF000;
uint32 *pt = (uint32 *) phys_to_virt(pt_phys);
if (!pt) continue;
```

**Explanation:**
- **pt_phys** = Physical address of the page table
  - `0xFFFFF000` = Keep bits [31:12], ignore lower 12 bits
  - This extracts the physical address of the page table

- **phys_to_virt()** = Converts physical address to virtual address
  - Typically: `va = pa + KERNEL_BASE`
  - Allows kernel to access the page table

- `if (!pt) continue;` = Skip if conversion failed

**Memory Structure:**
```
Page Directory Entry (PDE):
    [31:12] = Physical address of page table (20 bits)
    [11:0]  = Flags (PRESENT, WRITABLE, USER, etc.)
    
Example:
    PDE = 0x1000F000
    Physical page table = 0x1000F000 (bits [31:12])
```

---

### **Step 5: Iterate Through All Page Table Entries (PTEs)**
```c
for (uint32 ptx = 0; ptx < 1024; ++ptx) {
    uint32 pte = pt[ptx];
    if (!(pte & PERM_PRESENT)) continue;
```

**Explanation:**
- **ptx** = Page Table indeX (0 to 1023)
- **pte** = Page Table Entry
- Each page table has 1024 entries, each pointing to a 4KB physical page

**Memory Hierarchy:**
```
Page Directory (1024 entries)
    ↓ (for each PDE)
Page Tables (1024 entries each)
    ↓ (for each PTE)
Physical Pages (4KB each)
```

---

### **Step 6: Extract Physical Frame and Compare**
```c
uint32 frame_base = pte & 0xFFFFF000;
if (frame_base == pa_aligned) {
    return (pdx << 22) | (ptx << 12) | offset;
}
```

**Explanation:**
- **frame_base** = Physical address of the 4KB page
  - `0xFFFFF000` extracts bits [31:12]

- **Comparison:** `if (frame_base == pa_aligned)`
  - Does this PTE's physical frame match our target PA?

**If Match Found - Calculate VA:**
```
VA = (pdx << 22) | (ptx << 12) | offset

Where:
    pdx << 22   = Page directory index shifted to bits [31:22]
    ptx << 12   = Page table index shifted to bits [21:12]
    offset      = Original offset in bits [11:0]

Example:
    pdx = 1, ptx = 2, offset = 0x345
    VA = (1 << 22) | (2 << 12) | 0x345
       = 0x0400_0000 | 0x0000_2000 | 0x345
       = 0x0400_2345
```

---

## Virtual Address Structure (x86 32-bit)
```
Virtual Address (32 bits):
┌─────────────────┬─────────────────┬─────────────────┐
│  Dir Index      │ Table Index     │  Offset         │
│  (bits 31:22)   │ (bits 21:12)    │ (bits 11:0)     │
│  10 bits        │  10 bits        │  12 bits        │
└─────────────────┴─────────────────┴─────────────────┘

pdx = (VA >> 22) & 0x3FF       // Extract bits [31:22]
ptx = (VA >> 12) & 0x3FF       // Extract bits [21:12]
offset = VA & 0xFFF             // Extract bits [11:0]
```

---

## Complete Example Walkthrough

### **Scenario: Find VA for PA = 0x1000345**

**Step 1: Align and extract offset**
```
pa_aligned = 0x1000345 & ~0xFFF = 0x1000000
offset     = 0x1000345 &  0xFFF = 0x345
```

**Step 2: Loop through page directory**
```
pdx = 0, 1, 2, ... until we find the right entry
Let's say pdx = 4 has the right page table
```

**Step 3: Get page table from PDE**
```
pde = page_directory[4] = 0x2000F001
pt_phys = 0x2000F001 & 0xFFFFF000 = 0x2000F000
pt = (uint32*)phys_to_virt(0x2000F000)  // VA of page table
```

**Step 4: Loop through page table**
```
ptx = 0, 1, 2, ... until we find matching frame
Let's say ptx = 5 matches
```

**Step 5: Check if this PTE points to our PA**
```
pte = pt[5] = 0x1000F001
frame_base = 0x1000F001 & 0xFFFFF000 = 0x1000F000
Wait... this doesn't match 0x1000000

Let's say ptx = 1 matches instead:
pte = pt[1] = 0x1000001
frame_base = 0x1000001 & 0xFFFFF000 = 0x1000000 ✓ MATCH!
```

**Step 6: Calculate and return VA**
```
VA = (pdx << 22) | (ptx << 12) | offset
   = (4 << 22) | (1 << 12) | 0x345
   = 0x01000000 | 0x00001000 | 0x345
   = 0x01001345
```

---

## Return Value Possibilities

| Case | Return Value | Meaning |
|------|--------------|---------|
| **Found in 4MB page** | `(pdx << 22) \| (pa & 0x3FFFFF)` | 4MB page found |
| **Found in 4KB page** | `(pdx << 22) \| (ptx << 12) \| offset` | Normal page found |
| **Not found** | `0` | PA not mapped anywhere |

**Note:** Returning 0 is safe because:
- Virtual address 0 is always invalid/unmapped in kernel space
- So 0 can safely indicate "not found"

---

## Key Points Summary

1. **Scans entire page table structure** - Checks all 1024 PDEs and all PTEs
2. **Handles two page sizes** - 4MB pages (PSE) and 4KB pages (normal)
3. **Reconstructs VA** - Uses PDX, PTX, and offset to build the VA
4. **Slow but thorough** - O(n) operation, but guaranteed to find any mapped page
5. **Kernel-only function** - Used internally to map/track physical memory

---

## Advantages & Disadvantages

### ✅ Advantages
- Works with any page table setup
- No extra memory needed (no framesArr)
- Accurate (queries page tables directly)
- Handles both 4MB and 4KB pages

### ❌ Disadvantages
- **Slow:** Must iterate through ~1M entries in worst case
- **CPU intensive:** Lots of comparisons
- **Not ideal for frequent lookups**

---

## How to Use in Your Code

```c
// To find VA for a given PA:
uint32 physical_address = 0x1000000;
uint32 virtual_address = pa_to_va_scan(physical_address, ptr_page_directory);

if (virtual_address != 0) {
    printf("PA 0x%x maps to VA 0x%x\n", physical_address, virtual_address);
} else {
    printf("PA 0x%x is not mapped\n", physical_address);
}
```

This is perfect for your `kheap_virtual_address()` function!

