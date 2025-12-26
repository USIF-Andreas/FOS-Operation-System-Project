/*
 * channel.c
 *
 *  Created on: Sep 22, 2024
 *      Author: HP
 */
#include "channel.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <inc/string.h>
#include <inc/disk.h>
#include <kern/conc/kspinlock.h>

//===============================
// 1) INITIALIZE THE CHANNEL:
//===============================
// initialize its lock & queue
void init_channel(struct Channel *chan, char *name)
{
	strcpy(chan->name, name);
	init_queue(&(chan->queue));
}

//===============================
// 2) SLEEP ON A GIVEN CHANNEL:
//===============================
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// Ref: xv6-x86 OS code
void sleep(struct Channel *chan, struct kspinlock* lk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #1 CHANNEL - sleep
	//Your code is here
	//Comment the following line
	 struct Env* pros1 = get_cpu_proc();
	 if (pros1 == NULL) {
	     panic("this is error");
	 }

	    acquire_kspinlock(&ProcessQueues.qlock);


	    enqueue(&chan->queue, pros1);
	    pros1->env_status = ENV_BLOCKED;


	    release_kspinlock(lk);


	    sched();


	    acquire_kspinlock(lk);

	    release_kspinlock(&ProcessQueues.qlock);
	//panic("sleep() is not implemented yet...!!");
}

//==================================================
// 3) WAKEUP ONE BLOCKED PROCESS ON A GIVEN CHANNEL:
//==================================================
// Wake up ONE process sleeping on chan.
// The qlock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_one(struct Channel *chan)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #2 CHANNEL - wakeup_one
	//Your code is here
	acquire_kspinlock(&ProcessQueues.qlock);

	    uint32 siZe = queue_size(&chan->queue);
	    if (siZe > 0) {
	        struct Env *pros2 = dequeue(&chan->queue);
	        pros2->env_status = ENV_READY;
	        sched_insert_ready(pros2);
	    }



	    release_kspinlock(&ProcessQueues.qlock);
	//Comment the following line
	//panic("wakeup_one() is not implemented yet...!!");
}

//====================================================
// 4) WAKEUP ALL BLOCKED PROCESSES ON A GIVEN CHANNEL:
//====================================================
// Wake up all processes sleeping on chan.
// The queues lock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes

void wakeup_all(struct Channel *chan)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #3 CHANNEL - wakeup_all
	//Your code is here


	    acquire_kspinlock(&ProcessQueues.qlock);


	    struct Env* pros3;
	    while ((pros3 = dequeue(&chan->queue)) != NULL)
	    {
	        pros3->env_status = ENV_READY;
	        sched_insert_ready(pros3);
	    }


	    release_kspinlock(&ProcessQueues.qlock);


	//Comment the following line
	//panic("wakeup_all() is not implemented yet...!!");
}

