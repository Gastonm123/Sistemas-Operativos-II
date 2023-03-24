/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_simple.hh"
#include "system.hh"
#include "semaphore.hh"

#include <stdio.h>
#include <string.h>

#ifdef SEMAPHORE_TEST
Semaphore* s = nullptr;
#endif

/// Loop 10 times, yielding the CPU to another ready thread each iteration.
///
/// * `name` points to a string with a thread name, just for debugging
///   purposes.
void
SimpleThread(void *name_)
{
    // Reinterpret arg `name` as a string.
    char *name = (char *) name_;

    // If the lines dealing with interrupts are commented, the code will
    // behave incorrectly, because printf execution may cause race
    // conditions.
    for (unsigned num = 0; num < 10; num++) {
        #ifdef SEMAPHORE_TEST
		s->P();
		#endif
		printf("*** Thread `%s` is running: iteration %u\n", name, num);
        #ifdef SEMAPHORE_TEST
        s->V();
		#endif
		currentThread->Yield();
    }
    printf("!!! Thread `%s` has finished\n", name);
}

/// Set up a ping-pong between several threads.
///
/// Do it by launching one thread which calls `SimpleThread`, and finally
/// calling `SimpleThread` on the current thread.
void
ThreadTestSimple()
{
    #ifdef SEMAPHORE_TEST
	s = new Semaphore("s", 3);
	#end_if
    char const *names[] = {"2nd", "3rd", "4th", "5th"};
    char *name;
	for (unsigned num = 0; num < 4; num++) {
		name = new char[64];
		strncpy(name, names[num], 64);
    	Thread *newThread = new Thread(name);
    	newThread->Fork(SimpleThread, (void *) name);
	}
    SimpleThread((void *) "1st");
	// TODO: cuando puedo eliminar el semaphore?
	// delete s;
	// s = nullptr;
}
