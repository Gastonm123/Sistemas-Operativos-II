#ifndef NACHOS_VMEM_CORE_MAP__HH
#define NACHOS_VMEM_CORE_MAP__HH

#include "lib/list.hh"

class Thread;

class CoreMapEntry {
public:
    unsigned vpn;
    unsigned tid; 
};

class CoreMap {
public:
    CoreMap();
    ~CoreMap();
    unsigned MapPhysPage(unsigned vpn);
    void FreeAll(unsigned tid);

private:
    CoreMapEntry *coreMap;
    unsigned victim;
    unsigned FreePage();
    unsigned FindMatch(bool dirty);
};

#endif
