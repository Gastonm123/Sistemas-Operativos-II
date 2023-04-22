/// Routines for synchronizing threads.
///
/// The implementation for this primitive does not come with base Nachos.
/// It is left to the student.
///
/// When implementing this module, keep in mind that any implementation of a
/// synchronization routine needs some primitive atomic operation.  The
/// semaphore implementation, for example, disables interrupts in order to
/// achieve this; another way could be leveraging an already existing
/// primitive.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "lock.hh"
#include "semaphore.hh"
#include "system.hh"

/// Initialize a lock.
///
/// * `debugName` is an arbitrary name, useful for debugging.
Lock::Lock(const char *debugName)
{
    name = debugName;
    semaphore = new Semaphore(debugName, 1);
    holder = nullptr;
    prioInherit = false;
}

/// De-allocate lock.
Lock::~Lock()
{
    delete semaphore;
}

/// Get lock name. Useful for debugging purposes.
const char *
Lock::GetName() const
{
    return name;
}

/// Set priority inheritance flag for this lock.
void
Lock::SetPrioInherit()
{
    prioInherit = true;
}

/// Acquire lock. The lock may not be acquired while it is held by another
/// thread. A thread must not `Acquire` the lock if it is already holding it.
void
Lock::Acquire()
{
    ASSERT(!IsHeldByCurrentThread());
    if (holder) {
        if (prioInherit) {
            if (holder->GetPriority() > currentThread->GetPriority()) {
                unsigned holderPrio = holder->GetPriority();
                holder->Nice(currentThread->GetNice());
                scheduler->Reschedule(holder, holderPrio);
            }
        }
    }
    semaphore->P();
    holder = currentThread;
    savedPrio = currentThread->GetNice();
}

/// Release lock. Only a thread holding the lock may `Release` it.
void
Lock::Release()
{
    ASSERT(IsHeldByCurrentThread());
    if (prioInherit)
        holder->Nice(savedPrio);
    holder = nullptr;
    semaphore->V();
}

/// Returns `true` if the current thread is the one that possesses the
/// lock.
bool
Lock::IsHeldByCurrentThread() const
{
  return holder == currentThread;
}
