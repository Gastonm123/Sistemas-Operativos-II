#include "channel.hh"
#include "lock.hh"
#include "semaphore.hh"

Channel::Channel(const char *debugName)
{
    name = debugName;
    sendLock = new Lock("SEND");
    sendSem = new Semaphore("SEND", 0);
    receiveSem = new Semaphore("RECEIVE", 0);
}

Channel::~Channel()
{
    delete sendLock; 
    delete sendSem;
    delete receiveSem;
}

const char *Channel::GetName() const
{
    return name;
}

void Channel::Send(int message)
{
    sendLock->Acquire(); 
    buffer = message;
    sendSem->V();
    receiveSem->P();
    sendLock->Release();
}

void Channel::Receive(int *message)
{
   sendSem->P();
   *message = buffer;
   receiveSem->V();
}
