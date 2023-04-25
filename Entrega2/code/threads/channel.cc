#include "channel.hh"
#include "lock.hh"
#include "semaphore.hh"

Channel::Channel(const char *debugName)
{
    name = debugName;
    send_lock = new Lock("SEND");
    send_sem = new Semaphore("SEND", 0);
    receive_sem = new Semaphore("RECEIVE", 0);
}

Channel::~Channel()
{
    delete send_lock; 
    delete send_sem;
    delete receive_sem;
}

const char *Channel::GetName() const
{
    return name;
}

void Channel::Send(int message)
{
    send_lock->Acquire(); 
    buffer = message;
    send_sem->V();
    receive_sem->P();
    send_lock->Release();
}

void Channel::Receive(int *message)
{
   send_sem->P();
   *message = buffer;
   receive_sem->V();
}
