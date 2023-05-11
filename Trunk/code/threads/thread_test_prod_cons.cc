/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons.hh"
#include "system.hh"
#include "lock.hh"
#include "semaphore.hh"

#include <stdio.h>

static const unsigned NUM_ITEMS = 10;

static Lock* lock;
static Semaphore* sem;
static List<int>* buffer;

void
Producer(void*)
{
	for (int i = 0; i < NUM_ITEMS; ++i) {

		// Pass the current iteration number for testing purposes
		int message = i;

		lock->Acquire();
		buffer->Append(message);
		lock->Release();
		sem->V();
	}
	printf("Producer finished.\n");
}

void
Consumer(void*)
{
	for (int i = 0; i < NUM_ITEMS; ++i) {
		sem->P();
		lock->Acquire();
		int message = buffer->Pop();
		lock->Release();

		printf("Consumer received message %d\n", message);
	}
	printf("Consumer finished.\n");
}

void
ThreadTestProdCons()
{
	lock = new Lock("prod_cons lock");
	sem = new Semaphore("prod_cons semaphore", 0);
	buffer = new List<int>();

	Thread* producer = new Thread("producer", true);
	Thread* consumer = new Thread("consumer", true);

	producer->Fork(Producer, nullptr);
	consumer->Fork(Consumer, nullptr);

	producer->Join();
	consumer->Join();

	delete lock;
	delete sem;
	delete buffer;
}
