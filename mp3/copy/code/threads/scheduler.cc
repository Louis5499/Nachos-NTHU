// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    L1 = new List<Thread *>; 
    L2 = new List<Thread *>; 
    L3 = new List<Thread *>; 
    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete L1;
    delete L2;
    delete L3; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);

    int currentPriority = thread->GetPriority();
    if (currentPriority >= 100) {
        PutIntoQueue(1, L1, thread);
    } else if (currentPriority >= 50 && currentPriority <= 99) {
        PutIntoQueue(2, L2, thread);
    } else {
        PutIntoQueue(3, L3, thread);
    }
    thread->SetAgeInitialTick(kernel->stats->totalTicks);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    // DEBUG(dbgExpr, "[X] FindNextToRun");
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    /// shareable variable
    Thread *iterThread;
    ListIterator<Thread *> *iter;
    ///
    if (!L1->IsEmpty()) {
        // Preemptive SJF
        Thread *approximateThread = L1->Front(); // Assign default value (The first element) to prevent segmentation fault
        iter = new ListIterator<Thread *>(L1);
        for (; !iter->IsDone(); iter->Next()) {
            iterThread = iter->Item();
            if (iterThread->GetApproximateBurstTime() < approximateThread->GetApproximateBurstTime()) {
                approximateThread = iterThread;
            }
        }
        return RemoveFromQueue(1, L1, approximateThread);
    } else if (!L2->IsEmpty()) {
        // Non-preemptive priority
        Thread *highestPriorityThread = L2->Front();
        iter = new ListIterator<Thread *>(L2);
        for (; !iter->IsDone(); iter->Next()) {
            iterThread = iter->Item();
            if (iterThread->GetPriority() > highestPriorityThread->GetPriority()) {
                highestPriorityThread = iterThread;
            }
        }
        return RemoveFromQueue(2, L2, highestPriorityThread);
    } else if (!L3->IsEmpty()) {
        // Round-robin
        return RemoveFromQueue(3, L3, L3->Front());
    } else {
        return NULL;
    }

    // if (readyList->IsEmpty()) {
	// 	return NULL;
    // } else {
    // 	return readyList->RemoveFront();
    // }

}

Thread* Scheduler::PutIntoQueue(int layerIdx, List<Thread *> *cacheList, Thread *newThread) {
    cacheList->Append(newThread);
    DEBUG(dbgExpr, "[A] Tick ["<< kernel->stats->totalTicks <<"]: Thread [" << newThread->getID() << "] is inserted into queue L["<< layerIdx <<"]");
    // If L1
    //if (layerIdx == 1) PreemptiveCheck(newThread);
    return newThread;
}

Thread* Scheduler::RemoveFromQueue(int layerIdx, List<Thread *> *cacheList, Thread *newThread) {
    cacheList->Remove(newThread);
    DEBUG(dbgExpr, "[B] Tick ["<< kernel->stats->totalTicks <<"]: Thread [" << newThread->getID() << "] is removed from queue L["<< layerIdx <<"]");
    newThread->UpgradeTotalAgeTick(); // Calculate remaining tick from last check point, and add back to thread's total age.
    newThread->SetAgeInitialTick(kernel->stats->totalTicks); // Keep current tick data to thread struct, it's useful when this thread is transfered in aging rather than go to execute.
    return newThread;
}

// Executed when new thread is in L1
// void Scheduler::PreemptiveCheck(Thread *newThread) {
//     int currentThreadLayer = kernel->currentThread->GetLayer();
//     if (currentThreadLayer == 1) {
//         // If currrent thread is in L1, we need to compare current thread with new thread
//         if (newThread->GetApproximateBurstTime() < kernel->currentThread->GetApproximateBurstTime()) {
//             // Ready for Preemptive
//             DEBUG(dbgExpr,"[X] A Preemptive: " << newThread->getID() << " B: " << kernel->currentThread->getID());
//             kernel->interrupt->YieldOnReturn();
//         }
//     } else {
//         // If current thread is not in L1, we can directly preemptive current thread
//         kernel->interrupt->YieldOnReturn();
//     }
// }

void Scheduler::AgingProcess() {
    PerAgingProcess(L1, 1);
    PerAgingProcess(L2, 2);
    PerAgingProcess(L3, 3);
}

void Scheduler::PerAgingProcess(List<Thread *> *cacheList, int currentLayer) {
    ListIterator<Thread *> *iter;
    Thread *iterThread;
    iter = new ListIterator<Thread *>(cacheList);
    for (; !iter->IsDone(); iter->Next()) {
        iterThread = iter->Item();
        int foundPriority = iterThread->GetPriority();
        iterThread->UpgradeTotalAgeTick(); // Add 100 to thread's total age tick ()
        iterThread->SetAgeInitialTick(kernel->stats->totalTicks); // Keep current tick data to thread struct, it's useful when this thread is transfered to running state.
        bool isExceedAgeTime = iterThread->GetIsExceedAgeTime(); // Whether this thread total waiting tick is above 1500
        bool canStillAddPriority = foundPriority < 149;
        if (isExceedAgeTime && canStillAddPriority) {
            iterThread->DecreaseTotalAge(1500);
            iterThread->AccumulatePriority(10);
            DEBUG(dbgExpr, "[C] Tick ["<< kernel->stats->totalTicks <<"]: Thread [" << iterThread->getID() << "] changes its priority from ["<< foundPriority <<"] to ["<< iterThread->GetPriority() <<"]");
            // Manage L3->L2 L2->L1
            if (currentLayer == 3 && iterThread->GetPriority() >= 50) {
                RemoveFromQueue(3, L3, iterThread);
                PutIntoQueue(2, L2, iterThread);
            } else if (currentLayer == 2 && iterThread->GetPriority() >= 100) {
                RemoveFromQueue(2, L2, iterThread);
                PutIntoQueue(1, L1, iterThread);
            }
        }
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    DEBUG(dbgExpr, "[E] Tick ["<< kernel->stats->totalTicks <<"]: Thread ["<< nextThread->getID() <<"] is now selected for execution, thread ["<< oldThread->getID() <<"] is replaced, and it has executed ["<< oldThread->GetExecTick() << "] ticks");
    nextThread->SetInitialTick(kernel->stats->totalTicks);
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
    oldThread->SetInitialTick(kernel->stats->totalTicks); // Set initial tick 
    
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents in L1:\n";
    L1->Apply(ThreadPrint);
    cout << "Ready list contents in L2:\n";
    L2->Apply(ThreadPrint);
    cout << "Ready list contents in L3:\n";
    L3->Apply(ThreadPrint);
}
