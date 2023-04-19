/// Routines to choose the next thread to run, and to dispatch to that
/// thread.
///
/// These routines assume that interrupts are already disabled.  If
/// interrupts are disabled, we can assume mutual exclusion (since we are on
/// a uniprocessor).
///
/// NOTE: we cannot use `Lock`s to provide mutual exclusion here, since if we
/// needed to wait for a lock, and the lock was busy, we would end up calling
/// `FindNextToRun`, and that would put us in an infinite loop.
///
/// Very simple implementation -- no priorities, straight FIFO.  Might need
/// to be improved in later assignments.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "scheduler.hh"
#include "system.hh"
#include "lib/utility.hh"

#include <stdio.h>
#include <string.h>

const unsigned BITS_LONG = sizeof(long) * BITS_IN_BYTE;
const unsigned BITMAP_SIZE = DivRoundUp(MAX_PRIO, BITS_LONG);

/// Initialize the list of ready but not running threads to empty.
Scheduler::Scheduler()
{
    readyList = new List<Thread *>[MAX_PRIO];
    bitmap    = new unsigned long[BITMAP_SIZE];
    memset(bitmap, 0, BITMAP_SIZE);
}

/// De-allocate the list of ready threads.
Scheduler::~Scheduler()
{
    delete[] readyList;
    delete[] bitmap;
}

/// Set bit in the bitmap signaling ready threads.
inline void
Scheduler::SetBit(int bit)
{
    bitmap[bit/BITS_LONG] |= 1UL << (bit%BITS_LONG);
}

/// Reset bit in the bitmap signaling no more threads
inline void
Scheduler::ResetBit(int bit)
{
    bitmap[bit/BITS_LONG] &= ~(1UL << (bit%BITS_LONG));
}

/// Find first set bit in bitmap and return one plus its index or
/// zero if there is no set bit.
int
Scheduler::FindFirstBit()
{
    int idx;
    for (unsigned i = 0; i < BITMAP_SIZE; i++) {
        if ((idx = __builtin_ffsl(bitmap[i])))
            return (i * BITS_LONG) + idx;
    }
    return 0;
}

/// Mark a thread as ready, but not running.
/// Put it on the ready list, for later scheduling onto the CPU.
///
/// * `thread` is the thread to be put on the ready list.
void
Scheduler::ReadyToRun(Thread *thread)
{
    ASSERT(thread != nullptr);

    DEBUG('t', "Putting thread %s on ready list\n", thread->GetName());

    unsigned    priority = thread->GetPriority();
    List<Thread*> *queue = readyList + priority;

    thread->SetStatus(READY);
    queue->Append(thread);
    SetBit(priority);
}

/// Return the next thread to be scheduled onto the CPU.
///
/// If there are no ready threads, return null.
///
/// Side effect: thread is removed from the ready list.
Thread *
Scheduler::FindNextToRun()
{
    int idx;
    if ((idx = FindFirstBit())) {
        List<Thread*> *queue = readyList + idx - 1;
        ASSERT(!(queue->IsEmpty()));

        Thread *nextThread = queue->Pop();
        if (queue->IsEmpty())
            ResetBit(idx - 1);
        return nextThread;
    }
    return nullptr;
}

/// Dispatch the CPU to `nextThread`.
///
/// Save the state of the old thread, and load the state of the new thread,
/// by calling the machine dependent context switch routine, `SWITCH`.
///
/// Note: we assume the state of the previously running thread has already
/// been changed from running to blocked or ready (depending).
///
/// Side effect: the global variable `currentThread` becomes `nextThread`.
///
/// * `nextThread` is the thread to be put into the CPU.
void
Scheduler::Run(Thread *nextThread)
{
    ASSERT(nextThread != nullptr);

    Thread *oldThread = currentThread;

#ifdef USER_PROGRAM  // Ignore until running user programs.
    if (currentThread->space != nullptr) {
        // If this thread is a user program, save the user's CPU registers.
        currentThread->SaveUserState();
        currentThread->space->SaveState();
    }
#endif

    oldThread->CheckOverflow();  // Check if the old thread had an undetected
                                 // stack overflow.

    currentThread = nextThread;  // Switch to the next thread.
    currentThread->SetStatus(RUNNING);  // `nextThread` is now running.

    DEBUG('t', "Switching from thread \"%s\" to thread \"%s\"\n",
          oldThread->GetName(), nextThread->GetName());

    // This is a machine-dependent assembly language routine defined in
    // `switch.s`.  You may have to think a bit to figure out what happens
    // after this, both from the point of view of the thread and from the
    // perspective of the “outside world”.

    SWITCH(oldThread, nextThread);

    DEBUG('t', "Now in thread \"%s\"\n", currentThread->GetName());

    // If the old thread gave up the processor because it was finishing, we
    // need to delete its carcass.  Note we cannot delete the thread before
    // now (for example, in `Thread::Finish`), because up to this point, we
    // were still running on the old thread's stack!
    if (threadToBeDestroyed != nullptr) {
        delete threadToBeDestroyed;
        threadToBeDestroyed = nullptr;
    }

#ifdef USER_PROGRAM
    if (currentThread->space != nullptr) {
        // If there is an address space to restore, do it.
        currentThread->RestoreUserState();
        currentThread->space->RestoreState();
    }
#endif
}

/// Print the scheduler state -- in other words, the contents of the ready
/// list.
///
/// For debugging.
static void
ThreadPrint(Thread *t)
{
    ASSERT(t != nullptr);
    t->Print();
}

void
Scheduler::Print()
{
    printf("Ready list contents:\n");
    for (unsigned prio = 0; prio < MAX_PRIO; prio++) {
        if (!(readyList[prio].IsEmpty())) {
            printf("\n[%d] ", prio);
            readyList[prio].Apply(ThreadPrint);
        }
    }
}
