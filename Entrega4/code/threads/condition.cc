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


#include "condition.hh"
#include "system.hh"
#include "semaphore.hh"

/// Note -- without a correct implementation of `Condition::Wait`, the test
/// case in the network assignment will not work!

Condition::Condition(const char *debugName, Lock *conditionLock_)
{
    name = debugName;
    conditionLock = conditionLock_;
    queue = new PrioArray<Semaphore*>;
}

Condition::~Condition()
{
    delete queue;
}

const char *
Condition::GetName() const
{
    return name;
}

void
Condition::Wait()
{
    ASSERT(conditionLock->IsHeldByCurrentThread());

    Semaphore *semaphore = new Semaphore(name, 0);
    queue->Append(semaphore, currentThread->GetPriority());

    conditionLock->Release();
    semaphore->P();
    conditionLock->Acquire();
    
    delete semaphore;
}

void
Condition::Signal()
{
    ASSERT(conditionLock->IsHeldByCurrentThread());

    Semaphore *semaphore;
    if ((semaphore = queue->Pop())) {
        semaphore->V();
    }
}

void
Condition::Broadcast()
{
    ASSERT(conditionLock->IsHeldByCurrentThread());

    Semaphore *semaphore;
    while ((semaphore = queue->Pop())) {
        semaphore->V();
    }
}
