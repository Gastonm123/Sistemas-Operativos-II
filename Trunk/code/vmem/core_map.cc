/// Virtual memory functionality.

#include "core_map.hh"
#include "threads/system.hh"

/// Selects and moves a frame from main memory to swap.
///
/// Returns the frame number.
unsigned
CoreMap::MoveFrameToSwap() {
    /// Store recently used pages.
    AddressSpace *space = currentThread->space;
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    for (unsigned i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid) {
            space->pageTable[tlb[i].virtualPage].base = tlb[i];
        }
    }

    /// Clock algorithm requires at most four loops.
    for (unsigned loop = 0; loop < 4; loop++) {
        for (unsigned i = 0; i < NUM_PHYS_PAGES; i++) {
            CoreMapEntry *victimInfo = &coreMap[swapVictim];

            Thread *victimThread = threadMap->Get(victimInfo->tid);

            PageTableEntry *victimEntry =
                &victimThread->space->pageTable[victimInfo->vpn];

            bool suitable = false;
            switch (loop) {
                case 0:
                    suitable = (!victimEntry->base.use &&
                                !victimEntry->base.dirty);
                break;
                case 1:
                    /// On the second loop if a frame is unused it will be
                    /// dirty.
                    suitable = (!victimEntry->base.use);
                    victimEntry->base.use = 0;
                break;
                case 2:
                    suitable = (!victimEntry->base.dirty);
                break;
                default:
                    suitable = true;
                break;
            }

            if (suitable) {
                /// Invalidar la pagina si se encuentra en la TLB.
                if (victimThread == currentThread) {
                    for (unsigned j = 0; j < TLB_SIZE; j++) {
                        if (tlb[j].valid && tlb[j].virtualPage == victimInfo->vpn) {
                            tlb[j].valid = false;
                            break;
                        }
                    }
                }

                victimEntry->base.valid = false;

                if (victimEntry->base.dirty) {
                    victimThread->GetSwap()->WriteSwap(victimInfo->vpn,
                                                       swapVictim);
                    /// El disco guarda una copia de la ultima version de esta
                    /// pagina, asi que borramos el bit dirty.
                    victimEntry->base.dirty = false;
                    victimEntry->swap = true;
                }

                unsigned _swapVictim = swapVictim;
                swapVictim = (swapVictim + 1) % NUM_PHYS_PAGES;
                return _swapVictim;
            }

            swapVictim = (swapVictim + 1) % NUM_PHYS_PAGES;
        }
    }

    ASSERT(false); /// Not reached.
    return 0;
}

/// Finds an empty frame. If there is none, moves a frame to swap.
///
/// Returns an empty-frame number.
unsigned
CoreMap::Find()
{
    unsigned physicalPage = physPages->Find();
    if (physicalPage == -1) {
        physicalPage = CoreMap::MoveFrameToSwap();
    }
    return physicalPage;
}
