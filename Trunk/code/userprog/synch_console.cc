#include "synch_console.hh"


static void
ConsoleReadAvail(void *arg)
{
    ASSERT(arg != nullptr);
    SynchConsole *console = (SynchConsole *) arg;
    console->ReadAvail();
}

static void
ConsoleWriteDone(void *arg)
{
    ASSERT(arg != nullptr);
    SynchConsole *console = (SynchConsole *) arg;
    console->WriteDone();
}

SynchConsole::SynchConsole()
{
    readAvail = new Semaphore("synch console read", 0);
    writeDone = new Semaphore("synch console write", 0);
    readLock = new Lock("synch console read lock");
    writeLock = new Lock("synch console write lock");
    console = new Console(nullptr, nullptr, ConsoleReadAvail, ConsoleWriteDone, this);
}

SynchConsole::~SynchConsole()
{
    delete console;
    delete readLock;
    delete writeLock;
    delete readAvail;
    delete writeDone;
}

void
SynchConsole::Read(char* buffer, int size)
{
    ASSERT(buffer != nullptr);
    ASSERT(size > 0); // Por que size no es unsigned?

    readLock->Acquire();
    for (int i = 0; i < size; i++) {
        readAvail->P();
        buffer[i] = console->GetChar();
    }
    readLock->Release();
}

void
SynchConsole::Write(const char *buffer, int size)
{
    ASSERT(buffer != nullptr);
    ASSERT(size > 0); // Por que size no es unsigned?

    writeLock->Acquire();  
    for (int i = 0; i < size; i++) {
        console->PutChar(buffer[i]);
        writeDone->P();
    }
    writeLock->Release();
}

void
SynchConsole::ReadAvail()
{
    readAvail->V();
}

void
SynchConsole::WriteDone()
{
    writeDone->V();
}
