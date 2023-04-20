#include "thread_test_prio.hh"
#include "system.hh"
#include "lock.hh"

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

/// Simulate a weather thread.
void
Weather(void *dataBusLock_)
{
    Lock *dataBusLock = (Lock *) dataBusLock_;
    dataBusLock->Acquire();
    currentThread->Yield(); // simulates a real-time task arriving in the middle
                            // of the task.
    printf("*** Weather analyzed\n");
    dataBusLock->Release();
}

/// Simulate a communication thread.
void
Communication(void *arg)
{
    printf("*** Communications\n");
}

/// Simulate a data bus thread.
void
DataBus(void *dataBusLock_)
{
    Lock *dataBusLock = (Lock *) dataBusLock_;
    dataBusLock->Acquire(true);
    printf("*** Data bus liberated\n");
    dataBusLock->Release();
}

/// Set up the same conditions which caused a priority inversion in the Mars
/// Pathfinder mission. In this test a low-priority thread called Weather is
/// executed; it takes a lock and yields the processor. Then a medium-priority
/// thread (called Communication), a high-priority thread (called Data Bus),
/// and the low-priority thread are scheduled and the main thread yields the
/// processor. The high-priority thread will try to take the lock first and
/// then exit.
/// If an inversion occurs the results will be as follows:
/// *** Communications
/// *** Weather analyzed
/// *** Data bus liberated
///
/// Mars Pathfinder paper:
/// https://www.cs.unc.edu/~anderson/teach/comp790/papers/mars_pathfinder_short_version.html
void
ThreadTestInversion()
{
    Thread *weather = new Thread("Weather");
    Thread *communication = new Thread("Communication");
    Thread *dataBus = new Thread("Data Bus");

    // negative nice values so that they are executed before the main thread
    weather->Nice(-1); communication->Nice(-5);
    dataBus->Nice(-10);

    Lock *dataBusLock = new Lock("Data Bus Lock");
    weather->Fork(Weather, (void *) dataBusLock);
    currentThread->Yield();

    communication->Fork(Communication, NULL);
    dataBus->Fork(DataBus, (void *) dataBusLock);
    currentThread->Yield();

    delete dataBusLock;
    // threads get deleted when they exit
}
