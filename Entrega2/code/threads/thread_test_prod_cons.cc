/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons.hh"
#include "system.hh"
#include "channel.hh"

#include <stdio.h>

static const unsigned NUM_ITEMS = 10;

Channel* chan;

void
Producer(void*)
{
	for (int i = 0; i < NUM_ITEMS; ++i) {
		// random-looking numbers for testing purposes
		int message = (i * i) % 23;
		chan->Send(message);
	}
	printf("Producer finished.\n");
}

void
Consumer(void*)
{
	for (int i = 0; i < NUM_ITEMS; ++i) {
		int message;
		chan->Receive(&message);
		printf("Consumer received message %d\n", message);
	}
	printf("Consumer finished.\n");
}

void
ThreadTestProdCons()
{
	chan = new Channel("prod_cons channel");
	Thread* producer = new Thread("producer");
	Thread* consumer = new Thread("consumer");

	producer->Fork(Producer, nullptr);
	consumer->Fork(Consumer, nullptr);

	producer->Join();
	consumer->Join();

	delete chan;
}
