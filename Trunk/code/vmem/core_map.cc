#include "core_map.hh"
#include "threads/system.hh"

#ifdef USE_TLB
CoreMap::CoreMap() {
    coreMap = new List<CoreMapEntry *>();
}

CoreMap::~CoreMap() {
    while (!coreMap->IsEmpty()) {
        CoreMapEntry* entry = coreMap->Pop();
        delete entry;
    }
    delete coreMap;
}

void
CoreMap::RegisterPage(unsigned vpn, unsigned ppn) {
    CoreMapEntry *entry = new CoreMapEntry();
    entry->vpn = vpn;
    entry->ppn = ppn;
    entry->tid = currentThread->GetTid();
    coreMap->Append(entry);

    physPages->Mark(ppn);
}

unsigned
CoreMap::EvictPage() {
    ASSERT(!coreMap->IsEmpty());
    CoreMapEntry *entry;
    do {
        entry = coreMap->Pop();
        ASSERT(entry != nullptr);
    } while (entry->tid == -1);

    Thread *owner = threadMap->Get(entry->tid);
    ASSERT(owner != nullptr);
    owner->space->SwapPage(entry->vpn);

    unsigned ppn = entry->ppn;

    delete entry;
    return ppn;
}

unsigned
CoreMap::FindPhysPage() {
    unsigned ppn = physPages->Find();
    if (ppn == -1) {
        ppn = EvictPage();
    }
   return ppn;
}

void
InvalidateEntry(CoreMapEntry* entry) {
    ASSERT(entry != nullptr);
    unsigned currentTid = currentThread->GetTid();
    if (entry->tid == currentTid) {
        entry->tid = -1;
        physPages->Clear(entry->ppn);
    }
}

void
CoreMap::RemoveCurrentThread() {
    coreMap->Apply(InvalidateEntry);
}
#endif
