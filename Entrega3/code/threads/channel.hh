#ifndef NACHOS_THREADS_CHANNEL__HH
#define NACHOS_THREADS_CHANNEL__HH

class Semaphore;
class Lock;

class Channel {
public:
    Channel(const char *debugName);
    ~Channel();
    const char *GetName() const;

    void Send(int message);
    void Receive(int *message);

private:
    const char* name;
    Lock *sendLock;
    Semaphore *sendSem;
    Semaphore *receiveSem;
    int buffer;
};

#endif
