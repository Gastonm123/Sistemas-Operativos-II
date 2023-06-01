#include "core_map.hh"
#include "threads/system.hh"

#ifdef USE_TLB
CoreMap::CoreMap() {
    coreMap = new CoreMapEntry[NUM_PHYS_PAGES];
    victim = 0;
}

CoreMap::~CoreMap() {
    delete coreMap;
}


unsigned
CoreMap::MapPhysPage(unsigned vpn) {
    unsigned ppn = physPages->Find();
    if (ppn == -1) {
        ppn = FreePage();
    }

    coreMap[ppn].vpn = vpn;
    coreMap[ppn].tid = currentThread->GetTid();

    return ppn;
}

void
CoreMap::FreeAll(unsigned tid) {
    for (unsigned ppn = 0; ppn < NUM_PHYS_PAGES; ppn++) {
        if (coreMap[ppn].tid == tid) {
            physPages->Clear(ppn);
        }
    }
}

unsigned
CoreMap::FreePage() {
    currentThread->space->UpdatePageTable();

    unsigned ppn;
    ppn = FindMatch(false);
    if (ppn != -1) {
        goto EVICT_PAGE;
    }
    ppn = FindMatch(true);
    if (ppn != -1) {
        goto EVICT_PAGE;
    }
    ppn = FindMatch(false);
    if (ppn != -1) {
        goto EVICT_PAGE;
    }
    ppn = victim;
    victim = (victim + 1) % NUM_PHYS_PAGES;

EVICT_PAGE:
    Thread *owner = threadMap->Get(coreMap[ppn].tid);
    ASSERT(owner != nullptr);

    owner->space->SwapPage(coreMap[ppn].vpn);

    return ppn;
}

unsigned
CoreMap::FindMatch(bool dirty) {
    for (unsigned i = 0; i < NUM_PHYS_PAGES; i++) {
        unsigned ppn = victim;
        victim = (victim + 1) % NUM_PHYS_PAGES;

        CoreMapEntry *entry = &coreMap[ppn];

        unsigned vpn = entry->vpn;
        unsigned tid = entry->tid;

        Thread *owner = threadMap->Get(tid);
        ASSERT(owner != nullptr);

        bool useBit = owner->space->UseBit(vpn);
        bool dirtyBit = owner->space->DirtyBit(vpn);
        if (dirty) {
            // Si no encontramos una pagina limpia, las paginas usadas tienen
            // otra oportunidad.
            owner->space->ClearUseBit(vpn);
        }
        if (!useBit && dirty == dirtyBit) {
            return ppn;
        }
    }
    return -1;
}
#endif
