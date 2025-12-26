/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>
#include <kern/mem/kheap.h>

 //2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
 // 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE;
}
void setPageReplacmentAlgorithmCLOCK() { _PageRepAlgoType = PG_REP_CLOCK; }
void setPageReplacmentAlgorithmFIFO() { _PageRepAlgoType = PG_REP_FIFO; }
void setPageReplacmentAlgorithmModifiedCLOCK() { _PageRepAlgoType = PG_REP_MODIFIEDCLOCK; }
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal() { _PageRepAlgoType = PG_REP_DYNAMIC_LOCAL; }
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps) { _PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps; }
/*2024*/ void setFASTNchanceCLOCK(bool fast) { FASTNchanceCLOCK = fast; };
/*2025*/ void setPageReplacmentAlgorithmOPTIMAL() { _PageRepAlgoType = PG_REP_OPTIMAL; };

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE) { return _PageRepAlgoType == LRU_TYPE ? 1 : 0; }
uint32 isPageReplacmentAlgorithmCLOCK() { if (_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0; }
uint32 isPageReplacmentAlgorithmFIFO() { if (_PageRepAlgoType == PG_REP_FIFO) return 1; return 0; }
uint32 isPageReplacmentAlgorithmModifiedCLOCK() { if (_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0; }
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal() { if (_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0; }
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK() { if (_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0; }
/*2021*/ uint32 isPageReplacmentAlgorithmOPTIMAL() { if (_PageRepAlgoType == PG_REP_OPTIMAL) return 1; return 0; }

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt) { _EnableModifiedBuffer = enableIt; }
uint8 isModifiedBufferEnabled() { return _EnableModifiedBuffer; }

void enableBuffering(uint32 enableIt) { _EnableBuffering = enableIt; }
uint8 isBufferingEnabled() { return _EnableBuffering; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length; }
uint32 getModifiedBufferLength() { return _ModifiedBufferLength; }

//===============================
// FAULT HANDLERS
//===============================

//==================
// [0] INIT HANDLER:
//==================
void fault_handler_init()
{
	//setPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX);
	//setPageReplacmentAlgorithmOPTIMAL();
	setPageReplacmentAlgorithmCLOCK();
	//setPageReplacmentAlgorithmModifiedCLOCK();
	enableBuffering(0);
	enableModifiedBuffer(0);
	setModifiedBufferLength(1000);
}
//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault = 0;
extern uint32 sys_calculate_free_frames();

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe* tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//cprintf("************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else
	{
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		cprintf("\nFaulted VA = %x\n", fault_va);
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ((faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		faulted_env->tableFaultsCounter++;
		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
			/*============================================================================================*/
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here
			uint32 PPS = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
			if ((fault_va >= USER_LIMIT && fault_va < KERNEL_HEAP_MAX)) {
				env_exit();
				cprintf("if 1");
			}
			else if ((fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) && ((PPS & PERM_UHPAGE) != PERM_UHPAGE))
			{
				env_exit();
				cprintf("if 2");
			}
			else if (((PPS & PERM_PRESENT) == PERM_PRESENT) && ((PPS & PERM_WRITEABLE) != PERM_WRITEABLE))
			{
				env_exit();
				cprintf("if 3");
			}


			/*============================================================================================*/
		}

		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va);
		/*============================================================================================*/


		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter++;

		//				cprintf("[%08s] user PAGE fault va %08x\n", faulted_env->prog_name, fault_va);
		//				cprintf("\nPage working set BEFORE fault handler...\n");
		//				env_page_ws_print(faulted_env);
				//int ffb = sys_calculate_free_frames();

		if (isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			page_fault_handler(faulted_env, fault_va);
		}

		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(faulted_env);
		//		int ffa = sys_calculate_free_frames();
		//		cprintf("fault handling @%x: difference in free frames (after - before = %d)\n", fault_va, ffa - ffb);
	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}


//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env* curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//=========================
// [3] PAGE FAULT HANDLER:
//=========================
/* Calculate the number of page faults according th the OPTIMAL replacement strategy
 * Given:
 * 	1. Initial Working Set List (that the process started with)
 * 	2. Max Working Set Size
 * 	3. Page References List (contains the stream of referenced VAs till the process finished)
 *
 * 	IMPORTANT: This function SHOULD NOT change any of the given lists
 */
int get_optimal_num_faults(struct WS_List* initWorkingSet, int maxWSSize, struct PageRef_List* pageReferences)
{
	//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #2 get_optimal_num_faults
	//Your code is here
	//Comment the following line
	//panic("get_optimal_num_faults() is not implemented yet...!!");

	int yoyoCnt = 0;
	struct PageRefElement* yoyoP = NULL;
	int useless1=0;
	useless1++;	
	yoyoP = LIST_FIRST(pageReferences);
		while(yoyoP != NULL){
			uint32 yoyoA = yoyoP->virtual_address;
			int yoyoF = 0;
			
			struct WorkingSetElement* yoyoW = NULL;
			int tmp123=1;tmp123=tmp123*1;
			
			yoyoW = LIST_FIRST(initWorkingSet);
		
			while(yoyoW != NULL){
		
				if (yoyoW->virtual_address == yoyoA){
		
					yoyoF = 1;break;}
					yoyoW = LIST_NEXT(yoyoW);}
					if (!yoyoF){
						//cprintf("Optimal Fault at VA %x\n", yoyoA);
						yoyoCnt++;

		
					if (LIST_SIZE(initWorkingSet) < maxWSSize){
					//+bazot fe ws
					//cprintf("++++++++++ Adding VA %x to WS\n", yoyoA);
					struct WorkingSetElement* yoyoNew = (struct WorkingSetElement*)kmalloc(sizeof(struct WorkingSetElement));
					
					//cprintf("++++++++++ kmalloced at %x\n", yoyoNew);
					yoyoNew->virtual_address = yoyoA;
					//cprintf("++++++++++ assigned VA %x to WS element\n", yoyoNew->virtual_address);
					
					LIST_INSERT_TAIL(initWorkingSet, yoyoNew);
				}
			else{
				uint32 yoyoVR = 0;
				uint32 yoyoFD = 0;
				
				
				
				
				yoyoW = LIST_FIRST(initWorkingSet);
				
				while(yoyoW != NULL){
					uint32 yoyoDist = 0;
					
					struct PageRefElement* yoyoFuture = LIST_NEXT(yoyoP);
					while (yoyoFuture != NULL){
					
						if (yoyoFuture->virtual_address == yoyoW->virtual_address){break;}
					yoyoDist++;
					
					yoyoFuture = LIST_NEXT(yoyoFuture);
				}
					if (yoyoFuture == NULL){
						//cprintf("++++++++++ VA %x not found in future\n", yoyoW->virtual_address);
						uint32 ___MAXINT = 0xFFFFFFFF;
						yoyoDist = ___MAXINT;
					}
					
					if (yoyoDist > yoyoFD){
						yoyoFD = yoyoDist;
					
						yoyoVR = yoyoW->virtual_address;
				}
					yoyoW = LIST_NEXT(yoyoW);
				}
				struct WorkingSetElement* yoyoReplace = NULL;
				yoyoW = LIST_FIRST(initWorkingSet);
				while(yoyoW != NULL){
				
					if (yoyoW->virtual_address == yoyoVR){yoyoReplace = yoyoW;}
					yoyoW = LIST_NEXT(yoyoW);}
				
				int nothing=0;nothing++;}
			}
			yoyoP = LIST_NEXT(yoyoP);
}
	return yoyoCnt;
}


void page_fault_handler(struct Env* faulted_env, uint32 fault_va)
{
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL())
	{
		//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #1 Optimal Reference Stream
		//Your code is here
		//Comment the following line
		//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");

		uint32 yoyo_va = ROUNDDOWN(fault_va, PAGE_SIZE);
		static uint32 *yoyo_active = NULL;
		static int yoyo_active_cnt = 0;
		
		//just one
		static bool yoyo_init = 0;
		// 4 el function trace
		if (!yoyo_init)
		{
			yoyo_active = kmalloc(faulted_env->page_WS_max_size * sizeof(uint32));
			struct WorkingSetElement *yoyo_ws = LIST_FIRST(&faulted_env->page_WS_list);
			while (yoyo_ws != NULL)
			{
				pt_set_page_permissions(faulted_env->env_page_directory,
					yoyo_ws->virtual_address, 0, PERM_PRESENT);
				yoyo_ws = LIST_NEXT(yoyo_ws);
			}
			yoyo_init = 1;
			yoyo_active_cnt = 0;
		}

		// Check if page exists in active array
		bool yoyo_found = 0;
		for (int yoyo_i = 0; yoyo_i < yoyo_active_cnt; yoyo_i++)
		{
			if (yoyo_active[yoyo_i] == yoyo_va)
			{
				yoyo_found = 1;
				break;
			}
		}

		
		
		if (yoyo_found == 1)
		{
			//cprintf("********Optimal Hit at VA %x\n", yoyo_va);
			pt_set_page_permissions(faulted_env->env_page_directory, yoyo_va, PERM_PRESENT, 0);
			return;
		}

		
		struct PageRefElement *yoyo_ref = kmalloc(sizeof(struct PageRefElement));
		yoyo_ref->virtual_address = yoyo_va;
		LIST_INSERT_TAIL(&(faulted_env->referenceStreamList), yoyo_ref);

		//cprintf("Optimal Fault at VA %x\n", yoyo_va);
		if (yoyo_active_cnt >= faulted_env->page_WS_max_size)
		{
			//cprintf("Optimal Replacement needed\n");
			for (int yoyo_j = 0; yoyo_j < yoyo_active_cnt; yoyo_j++)
			{
				pt_set_page_permissions(faulted_env->env_page_directory, yoyo_active[yoyo_j], 0, PERM_PRESENT);
			}
			yoyo_active_cnt = 0;
		}

		
		uint32 *yoyo_pt = NULL;
		struct FrameInfo *yoyo_frame = get_frame_info(faulted_env->env_page_directory, yoyo_va, &yoyo_pt);

		if (!yoyo_frame)
		{
			
			allocate_frame(&yoyo_frame);
			map_frame(faulted_env->env_page_directory, yoyo_frame, yoyo_va, PERM_PRESENT | PERM_USER | PERM_WRITEABLE);
			
			int yoyo_rd = pf_read_env_page(faulted_env, (void*)yoyo_va);
			if (yoyo_rd == E_PAGE_NOT_EXIST_IN_PF)
			{
				if ((yoyo_va >= USTACKBOTTOM && yoyo_va < USTACKTOP) ||
					(yoyo_va >= USER_HEAP_START && yoyo_va < USER_HEAP_MAX))
				{
					memset((void*)yoyo_va, 0, PAGE_SIZE);
				}
				else
				{
					env_exit();
				}
			}
		}
		else
		{
			
			pt_set_page_permissions(faulted_env->env_page_directory, yoyo_va, PERM_PRESENT, 0);
		}
		//cprintf("Page fault handled for VA %x\n", yoyo_va);
		yoyo_active[yoyo_active_cnt] = yoyo_va;
		yoyo_active_cnt++;

		return;
	}
	else
	{
		struct WorkingSetElement* victimWSElement = NULL;
		uint32 wsetSize = LIST_SIZE(&(faulted_env->page_WS_list));

		if (wsetSize < (faulted_env->page_WS_max_size))
		{
			uint32 newVirtualAddress = ROUNDDOWN(fault_va, PAGE_SIZE);
		
			struct FrameInfo* newFrame = NULL;
			allocate_frame(&newFrame);
		
			uint32 perms = PERM_PRESENT | PERM_USER | PERM_WRITEABLE;
			if (newVirtualAddress >= USER_HEAP_START && newVirtualAddress < USER_HEAP_MAX)
				perms |= PERM_UHPAGE;
		
			map_frame(faulted_env->env_page_directory, newFrame, newVirtualAddress, perms);
		
			int readPage = pf_read_env_page(faulted_env, (void*)newVirtualAddress);
		
			if (readPage == E_PAGE_NOT_EXIST_IN_PF)
			{
				if ((newVirtualAddress >= USTACKBOTTOM && newVirtualAddress < USTACKTOP) ||
					(newVirtualAddress >= USER_HEAP_START && newVirtualAddress < USER_HEAP_MAX))
				{
					memset((void*)newVirtualAddress, 0, PAGE_SIZE);
				}
				else
				{
					env_exit();
					return;
				}
			}
			else if (readPage != 0)
			{
				panic("PLACEMENT: pf_read_env_page returned error %d", readPage);
			}
		
			struct WorkingSetElement* newElement =
				env_page_ws_list_create_element(faulted_env, newVirtualAddress);
		
			bool isWS_WasFullBefore = 0;
			if (faulted_env->page_last_WS_element != NULL)
				isWS_WasFullBefore = 1;
				//cprintf("WAS FULL = %d\n", isWS_WasFullBefore);
				//cprintf("PLACEMENT: Inserting new WSE for VA %x\n", newVirtualAddress);
			if (isWS_WasFullBefore)
			{
				//cprintf("PLACEMENT: Inserting new WSE for VA %x\n", newVirtualAddress);
				LIST_INSERT_BEFORE(&(faulted_env->page_WS_list),
								faulted_env->page_last_WS_element, newElement);
			}
			else
			{
				LIST_INSERT_TAIL(&(faulted_env->page_WS_list), newElement);
				//cprintf("newELement=%x\n", newElement);
				if (faulted_env->page_WS_max_size != wsetSize + 1)
					faulted_env->page_last_WS_element = NULL;
				else
					faulted_env->page_last_WS_element =
						LIST_FIRST(&(faulted_env->page_WS_list));
			}

					//Comment the following line
					// panic("page_fault_handler().PLACEMENT is not implemented yet...!!");
				}
		else{
		if (isPageReplacmentAlgorithmCLOCK())
			{
				//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - Clock Replacement
				//Your code is here

				//bool unfoundVictim = 0;
				struct WorkingSetElement* handPointer = faulted_env->page_last_WS_element;
				if (handPointer == NULL)
				{
					handPointer = LIST_FIRST(&faulted_env->page_WS_list);
				}

				struct WorkingSetElement* currentPage = handPointer;

			    
				for (uint32 yoyo_i = 0; yoyo_i < wsetSize * 2; yoyo_i++)
				{
					uint32 pagePermissions = pt_get_page_permissions(faulted_env->env_page_directory, currentPage->virtual_address);
					uint32 usedbit = pagePermissions & PERM_USED;
					//cprintf("Clock: Inspecting VA %x, used bit = %d\n", currentPage->virtual_address, usedbit);
					if (usedbit == 0)
					{
						// Found victim - page with used bit = 0
						//cprintf("Clock: Victim found at VA %x\n", currentPage->virtual_address);
						victimWSElement = currentPage;
						break;
					}
					else
					{
						
						pt_set_page_permissions(faulted_env->env_page_directory, currentPage->virtual_address, 0, PERM_USED);
						
					}

					
					currentPage = LIST_NEXT(currentPage);
					//cprintf("currentPage=%x\n", currentPage);
					if (currentPage == NULL)
					{
						//cprintf("Wrapping around to first element of page_WS_list\n");
						currentPage = LIST_FIRST(&faulted_env->page_WS_list);
					}
				}

				
				uint32 oldVirtualAdd = victimWSElement->virtual_address;
				uint32 newVirtualAdd = ROUNDDOWN(fault_va, PAGE_SIZE);

				uint32* tmpPageTable = NULL;
				struct FrameInfo* victFrame = get_frame_info(faulted_env->env_page_directory, oldVirtualAdd, &tmpPageTable);
				// mmkn ybka be null
				if (victFrame == NULL)
				{
					//unfoundVictim = 1;
					panic("get_frame_info returned NULL on victim");
				}

			
				uint32 victPermissions = pt_get_page_permissions(faulted_env->env_page_directory, oldVirtualAdd);
				if (victPermissions & PERM_MODIFIED)
				{
					int updatePage = pf_update_env_page(faulted_env, oldVirtualAdd, victFrame);
					if (updatePage == E_NO_PAGE_FILE_SPACE)
					{
						//unfoundVictim = 1;
						panic("no page file space");
					}
				}

				
				unmap_frame(faulted_env->env_page_directory, oldVirtualAdd);
				pt_clear_page_table_entry(faulted_env->env_page_directory, oldVirtualAdd);
				
				tlb_invalidate(faulted_env->env_page_directory, (void*)oldVirtualAdd);
				//cprintf("Unmapped frame from VA %x\n", oldVirtualAdd);
				
				// b mk frame we b mapping
				allocate_frame(&victFrame);
				map_frame(faulted_env->env_page_directory, victFrame, newVirtualAdd,
					PERM_PRESENT | PERM_USER | PERM_WRITEABLE | PERM_UHPAGE);

				// b read mn page file
				int redPage = pf_read_env_page(faulted_env, (void*)newVirtualAdd);
				//cprintf("Clock Fault at VA %x\n", newVirtualAdd);
				if (redPage == E_PAGE_NOT_EXIST_IN_PF)
				{
					//cprintf("Clock Fault at VA %x\n", newVirtualAdd);
					memset((void*)newVirtualAdd, 0, PAGE_SIZE);
				}
				else if (redPage != 0)
				{
					//cprintf(" pf_read_env_page returned error %d\n", redPage);
					//unfoundVictim = 1;
					panic("pf_read_env_page returned error %d", redPage);
				}

				victimWSElement->virtual_address = newVirtualAdd;
				
				victimWSElement->empty = 0;

				
				struct WorkingSetElement* nxtElement = LIST_NEXT(victimWSElement);
				if (nxtElement == NULL)
				{
					nxtElement = LIST_FIRST(&faulted_env->page_WS_list);
				}
				faulted_env->page_last_WS_element = nxtElement;

				
				pt_set_page_permissions(faulted_env->env_page_directory, newVirtualAdd, PERM_USED, 0);
			}
			else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX))
			{
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #2 LRU Aging Replacement
				//Your code is here

				struct WorkingSetElement *ws=LIST_FIRST(&faulted_env->page_WS_list);
				unsigned int mTS=~0U;

				while(ws!=NULL){
					if(!ws->empty&&ws->time_stamp<mTS){mTS=ws->time_stamp;victimWSElement=ws;}
					ws = LIST_NEXT(ws);}

				uint32 oVA=victimWSElement->virtual_address;
				uint32 nVA=ROUNDDOWN(fault_va,PAGE_SIZE);

				uint32 *pT=NULL;
				struct FrameInfo *vF=get_frame_info(faulted_env->env_page_directory,oVA,&pT);

				uint32 vP=pt_get_page_permissions(faulted_env->env_page_directory,oVA);
				if(vP&PERM_MODIFIED){
					if(pf_update_env_page(faulted_env,oVA,vF)==E_NO_PAGE_FILE_SPACE){panic("PF full!");}
						pt_set_page_permissions(faulted_env->env_page_directory, oVA, 0, PERM_MODIFIED);}

				unmap_frame(faulted_env->env_page_directory,oVA);
				pt_clear_page_table_entry(faulted_env->env_page_directory,oVA);
				tlb_invalidate(faulted_env->env_page_directory,(void*)oVA);

				allocate_frame(&vF);
				map_frame(faulted_env->env_page_directory,vF,nVA,PERM_PRESENT|PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);

				int rP=pf_read_env_page(faulted_env,(void*)nVA);
				if(rP==E_PAGE_NOT_EXIST_IN_PF){memset((void*)nVA,0,PAGE_SIZE);}

				victimWSElement->virtual_address=nVA;
				victimWSElement->empty=0;
				victimWSElement->time_stamp=(1U<<31);

				pt_set_page_permissions(faulted_env->env_page_directory,victimWSElement->virtual_address,PERM_USED,0);

				//Comment the following line
			    //panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			}
			else if (isPageReplacmentAlgorithmModifiedCLOCK())
			{
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #3 Modified Clock Replacement
				//Your code is here

				struct WorkingSetElement *hP=faulted_env->page_last_WS_element;
				if(hP==NULL)hP=LIST_FIRST(&faulted_env->page_WS_list);

				struct WorkingSetElement *cP=hP;
				uint32 pU=PERM_USED;
				uint32 pM=PERM_MODIFIED;

				do{uint32 pPerm=pt_get_page_permissions(faulted_env->env_page_directory,cP->virtual_address);
					if(!(pPerm&pU)&&!(pPerm&pM)){victimWSElement=cP;break;}
					cP=LIST_NEXT(cP);
				    if(!cP)cP=LIST_FIRST(&faulted_env->page_WS_list);}while(cP!=hP);

				if(!victimWSElement){
					for(uint32 k=0;k<wsetSize;k++){
						uint32 pPerm=pt_get_page_permissions(faulted_env->env_page_directory,cP->virtual_address);
						if (!(pPerm&pU)){victimWSElement=cP;break;}
						pt_set_page_permissions(faulted_env->env_page_directory,cP->virtual_address,0,pU);
						cP=LIST_NEXT(cP);
						if(!cP)cP=LIST_FIRST(&faulted_env->page_WS_list);}}

				if(!victimWSElement){
					uint32 c=0;
					while(c<wsetSize){
						uint32 pPerm=pt_get_page_permissions(faulted_env->env_page_directory,cP->virtual_address);
						if(!(pPerm&pU)&&!(pPerm&pM)){victimWSElement=cP;break;}
						cP=LIST_NEXT(cP);
						if(!cP)cP=LIST_FIRST(&faulted_env->page_WS_list);}}

				if(!victimWSElement){
					uint32 d=0;
					do{uint32 pPerm=pt_get_page_permissions(faulted_env->env_page_directory,cP->virtual_address);
					    if(!(pPerm&pU)){victimWSElement=cP;break;}
						pt_set_page_permissions(faulted_env->env_page_directory,cP->virtual_address,0,pU);
						cP=LIST_NEXT(cP);
						if(!cP)cP=LIST_FIRST(&faulted_env->page_WS_list);
						d++;} while(d<wsetSize);}

				uint32 oVA=victimWSElement->virtual_address;
				uint32 nVA=ROUNDDOWN(fault_va,PAGE_SIZE);

				uint32 *pT=NULL;
				struct FrameInfo *vF=get_frame_info(faulted_env->env_page_directory,oVA,&pT);

				uint32 vP=pt_get_page_permissions(faulted_env->env_page_directory,oVA);
				if(vP&pM){if(pf_update_env_page(faulted_env,oVA,vF) == E_NO_PAGE_FILE_SPACE){panic("PF full!");}
						  pt_set_page_permissions(faulted_env->env_page_directory,oVA,0,pM);}

				unmap_frame(faulted_env->env_page_directory, oVA);
				pt_clear_page_table_entry(faulted_env->env_page_directory, oVA);
				tlb_invalidate(faulted_env->env_page_directory, (void*)oVA);

				allocate_frame(&vF);
				map_frame(faulted_env->env_page_directory,vF,nVA,PERM_PRESENT|PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);

				int rP=pf_read_env_page(faulted_env,(void*)nVA);
				if(rP==E_PAGE_NOT_EXIST_IN_PF){memset((void*)nVA,0,PAGE_SIZE);}

				victimWSElement->virtual_address=nVA;
				victimWSElement->empty=0;

				struct WorkingSetElement *np=LIST_NEXT(victimWSElement);
				if(!np)np=LIST_FIRST(&faulted_env->page_WS_list);
				faulted_env->page_last_WS_element=np;

				pt_set_page_permissions(faulted_env->env_page_directory,victimWSElement->virtual_address,pU,0);

				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			}
		}
	}
	}
#endif



void __page_fault_handler_with_buffering(struct Env* curenv, uint32 fault_va)
{
	panic("this function is not required...!!");
}


