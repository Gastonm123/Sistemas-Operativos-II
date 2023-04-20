/// Data structures for the thread dispatcher and scheduler.
///
/// Primarily, the list of threads that are ready to run.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_THREADS_SCHEDULER__HH
#define NACHOS_THREADS_SCHEDULER__HH


#include "thread.hh"
#include "lib/list.hh"

static const unsigned MAX_PRIO = 140;

/// The following class defines the scheduler/dispatcher abstraction --
/// the data structures and operations needed to keep track of which
/// thread is running, and which threads are ready but not running.
class Scheduler {
public:

    /// Initialize list of ready threads.
    Scheduler();

    /// De-allocate ready list.
    ~Scheduler();

    /// Thread can be dispatched.
    void ReadyToRun(Thread *thread);

    /// Dequeue first thread on the ready list, if any, and return thread.
    Thread *FindNextToRun();

    /// Cause `nextThread` to start running.
    void Run(Thread *nextThread);

    /// Reschedule`thread` according to its new priority.
    void Reschedule(Thread *thread, unsigned oldPrio);

    // Print contents of ready list.
    void Print();

private:
    
    List<Thread*> *readyList;
    unsigned long *bitmap;

    void SetBit(int bit);
    void ResetBit(int bit);
    int FindFirstBit();

};


#endif
