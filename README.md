# 🖥️ FOS - Operating System

**FOS** is an educational operating system kernel for the x86 architecture, featuring advanced memory management with **O(1) kernel heap allocation**, multiple page replacement algorithms, and comprehensive virtual memory support.

---

## ✨ Key Features

### 🧠 Kernel Heap (kheap) - O(1) Allocation

#### `kmalloc()` - Constant Time Allocation
- **Dual-Strategy Allocator**:
  - Small blocks (≤ 2KB): Dynamic block allocator with Custom Fit strategy
  - Large blocks (> 2KB): Page-level cluster allocation with **O(1)** lookup
- **Cluster-Based Free List**: Array of 1024 linked lists indexed by cluster size
  - `free_clusters[size]` → Direct access to available clusters of exact size
  - `max_FREE_cluster[]` tracks largest available cluster for Worst-Fit fallback
- **Frame Tracking**: `framesArr[]` maps physical frames to virtual addresses for instant PA→VA translation

#### `kfree()` - Constant Time Deallocation with Coalescing
- **Boundary Tag Coalescing**: `cluster_size[]` array stores cluster metadata at both ends
  - Negative values = allocated, Positive values = free
  - Enables **O(1)** neighbor detection and merging
- **Immediate Coalescing**: Adjacent free clusters merge automatically
- **Smart List Management**: Freed clusters inserted at list head for O(1) insertion

#### `krealloc()` - Efficient Reallocation
- In-place resize when possible
- Automatic migration for growing allocations

┌─────────────────────────────────────────────────────────────┐
│ KERNEL HEAP LAYOUT │
├─────────────────────────────────────────────────────────────┤
│ KERNEL_HEAP_START │◄─── Dynamic Allocator (≤2KB blocks) │
│ ↓ │ - First Fit / Best Fit / Custom │
│ dynAllocEnd │ │
├───────────────────────┼─────────────────────────────────────┤
│ kheapPageAllocStart │◄─── Page Allocator (>2KB) │
│ ↓ │ - Cluster-based O(1) allocation │
│ kheapPageAllocBreak │ - free_clusters[1024] lists │
│ ↓ │ │
│ KERNEL_HEAP_MAX │ │
└─────────────────────────────────────────────────────────────┘



### ⚡ Page Fault Handler - Multiple Replacement Algorithms

#### 🔄 Clock Algorithm (Second Chance)
- **Hardware-assisted**: Uses CPU's `PERM_USED` bit
- **Single-pass replacement**: Hand pointer traverses working set circularly
- **Lazy write-back**: Only writes modified pages (`PERM_MODIFIED`) to page file
- **Implementation**: Resets used bit on first encounter, evicts on second

#### 🎯 Optimal Algorithm (Bélády's Algorithm)
- **Reference Stream Tracking**: Records all page references for analysis
- **Farthest Future Use**: Evicts page that won't be used for longest time
- **Theoretical Baseline**: Used for comparing other algorithms' performance
- **Active Set Management**: Maintains array of currently resident pages

#### ⏰ LRU (Least Recently Used) - Time Approximation
- **Timestamp-based**: Each page tracks last access time
- **Aging mechanism**: Finds page with minimum timestamp for eviction

#### 🔀 Modified Clock (Enhanced Second Chance)
- **Two-bit classification**: Considers both Used and Modified bits
- **Priority eviction order**:
  1. `(U=0, M=0)` - Not used, not modified (best victim)
  2. `(U=0, M=1)` - Not used, but modified
  3. `(U=1, M=0)` - Used, not modified
  4. `(U=1, M=1)` - Used and modified (worst victim)

---

### 💾 Virtual Memory System

┌────────────────────────────────────────────────────────────┐
│ VIRTUAL MEMORY MAP │
├────────────────────────────────────────────────────────────┤
│ 4GB ────────► Invalid Memory │
│ KERNEL_HEAP_MAX Kernel Heap (kheap) [RW/--] │
│ KERNEL_HEAP_START │
│ ────────────► Remapped Physical Memory [RW/--] │
│ KERNEL_BASE (0xF0000000) │
│ ────────────► Page Tables (VPT) [RW/--] │
│ ────────────► Scheduler Kernel Stacks │
│ USER_TOP ────► User Stack [RW/RW] │
│ USER_HEAP_MAX User Heap [RW/RW] │
│ USER_HEAP_START │
│ 0 ──────────► User Code/Data [R-/RW] │
└────────────────────────────────────────────────────────────┘

---

## 🚀 Quick Start

```bash
#After Instal WSL and qemu
# Build
make clean && make

# Run in QEMU
make qemu

# Run in Bochs
[bochscon.bat](http://_vscodecontentref_/0)  # or bochs -f bochsrc
