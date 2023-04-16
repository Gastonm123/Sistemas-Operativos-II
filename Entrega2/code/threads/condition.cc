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

#include "semaphore.hh"

/// Note -- without a correct implementation of `Condition::Wait`, the test
/// case in the network assignment will not work!

Condition::Condition(const char *debugName, Lock *conditionLock)
{
	this->conditionLock = conditionLock;
	queue = new List<Semaphore*>();
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
	Semaphore* semaphore = new Semaphore("Wait", 0);
	Enqueue(semaphore);
	semaphore->P();
	delete semaphore;
}

void
Condition::Signal()
{
	auto semaphore = Dequeue();
	semaphore->V();
}

void
Condition::Broadcast()
{
	while (!QueueIsEmpty()) {
		Signal();
	}
}

void
Condition::Enqueue(Semaphore* semaphore)
{
	queue->Append(semaphore);
}

Semaphore*
Condition::Dequeue()
{
	return queue->Pop();
}

bool
Condition::QueueIsEmpty()
{
	return queue->IsEmpty();
}
