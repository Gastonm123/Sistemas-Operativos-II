#include "thread_test_prio.hh"
#include "system.hh"

#include <stdio.h>
#include <string.h>

/// Loop 10 times, yielding the CPU to another ready thread each iteration.
///
/// * `name` points to a string with a thread name, just for debugging
///   purposes.
void
SimpleSimpleThread(void *name_)
{
	// Reinterpret arg `name` as a string.
	char *name = (char *) name_;

	// If the lines dealing with interrupts are commented, the code will
	// behave incorrectly, because printf execution may cause race
	// conditions.
	for (unsigned num = 0; num < 10; num++) {
		printf("*** Thread `%s` is running: iteration %u\n", name, num);
		currentThread->Yield();
	}
	printf("!!! Thread `%s` has finished\n", name);
}

/// Set up a ping-pong between several threads but with priorities.
/// The result should be that the higher priority threads finish before any
/// lower priority thread starts.
void
ThreadTestPrio()
{
    char const *names[] = {"2nd", "3rd", "4th", "5th"};
    char *name;
    for (unsigned num = 0; num < 4; num++) {
        name = new char[64];
        strncpy(name, names[num], 64);
        Thread *newThread = new Thread(name);
        newThread->Nice(num);
        newThread->Fork(SimpleSimpleThread, (void *) name);
    }
    currentThread->Nice(19);
    SimpleSimpleThread((void *) "1st");
}



