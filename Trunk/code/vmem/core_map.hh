#ifndef NACHOS_VMEM_CORE_MAP__HH
#define NACHOS_VMEM_CORE_MAP__HH

#include "lib/list.hh"

class Thread;

class CoreMapEntry {
public:
    unsigned vpn;
    unsigned ppn;
    Thread* thread;
};

class CoreMap {
public:
    CoreMap();
    ~CoreMap();
    unsigned FindPhysPage();
    void RegisterPage(unsigned vpn, unsigned ppn);
    void RemoveCurrentThread();

private:
    List<CoreMapEntry *> *coreMap; 
    unsigned EvictPage();
};

#endif
