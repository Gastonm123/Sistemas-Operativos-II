#ifndef NACHOS_USERPROG_SYNCHCONSOLE_HH
#define NACHOS_USERPROG_SYNCHCONSOLE_HH


#include "machine/console.hh"
#include "threads/lock.hh"
#include "threads/semaphore.hh"


class SynchConsole {
public:

    SynchConsole();

    ~SynchConsole();

    void Read(char* buffer, int size);
    void Write(const char* buffer, int size);
    
    void ReadAvail();
    void WriteDone();

private:
    Console *console;
    Semaphore *readAvail, *writeDone;
    Lock *readLock, *writeLock;
};


#endif
