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
    entry->thread = currentThread;
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
    } while (entry->thread == nullptr);

    entry->thread->space->SwapPage(entry->vpn);

    unsigned ppn = entry->ppn;
    physPages->Clear(ppn);

    delete entry;
    return ppn;
}

unsigned
CoreMap::FindPhysPage() {
    unsigned ppn = physPages->Find();
    if (ppn == -1) {
        ppn = EvictPage();
        // TODO: hace falta?
        physPages->Mark(ppn);
    }
   return ppn;
}

void
InvalidateEntry(CoreMapEntry* entry) {
    ASSERT(entry != nullptr);
    if (entry->thread == currentThread) {
        entry->thread = nullptr;
        physPages->Clear(entry->ppn);
    }
}

void
CoreMap::RemoveCurrentThread() {
    coreMap->Apply(InvalidateEntry);
}
#endif
