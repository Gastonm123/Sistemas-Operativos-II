/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"
#include "lib/utility.hh"

#include <string.h>

#ifdef USE_TLB
#include "vmem/swap.hh"
#include "vmem/core_map.hh"
#endif

uint32_t TranslatePage(uint32_t virtualPage, TranslationEntry const* pageTable) {
    uint32_t physicalPage = pageTable[virtualPage].physicalPage;

    return physicalPage;
}

uint32_t TranslateAddress(uint32_t virtualAddress, TranslationEntry const* pageTable) {
    uint32_t virtualPage = virtualAddress / PAGE_SIZE;
    uint32_t offset      = virtualAddress % PAGE_SIZE;

    uint32_t physicalPage = TranslatePage(virtualPage, pageTable);
    uint32_t physicalAddr = physicalPage * PAGE_SIZE + offset;

    return physicalAddr;
}

/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executableFile, unsigned asid)
{
#ifdef USE_TLB
    ASSERT(executableFile != nullptr);

    exe  = new Executable(executableFile);
    ASSERT(exe->CheckMagic());

    this->asid = asid;
    swap = new Swap(asid);

    unsigned size = exe->GetSize() + USER_STACK_SIZE;

    numPages = DivRoundUp(size, PAGE_SIZE);
    size = numPages * PAGE_SIZE;

    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; ++i) {
        pageTable[i].virtualPage  = i;
        pageTable[i].physicalPage = 0;
        pageTable[i].valid        = false;
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
        pageTable[i].swap         = false; 
    }

    /// Initialize tlbVictim
    tlbVictim = 0;

#else
    ASSERT(executableFile != nullptr);

    Executable exe (executableFile);
    ASSERT(exe.CheckMagic());

    // How big is address space?

    unsigned size = exe.GetSize() + USER_STACK_SIZE;
      // We need to increase the size to leave room for the stack.
    numPages = DivRoundUp(size, PAGE_SIZE);
    size = numPages * PAGE_SIZE;

    ASSERT(numPages <= physPages->CountClear());
      // Check we are not trying to run anything too big -- at least until we
      // have virtual memory.

    DEBUG('a', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // First, set up the translation.

    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        pageTable[i].physicalPage = physPages->Find();
        pageTable[i].valid        = true;
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
        pageTable[i].swap         = false;
          // If the code segment was entirely on a separate page, we could
          // set its pages to be read-only.
    }

    char *mainMemory = machine->GetMMU()->mainMemory;

    // Then, copy in the code and data segments into memory.
    uint32_t const codeSize = exe.GetCodeSize();
    uint32_t initDataSize = exe.GetInitDataSize();
    uint32_t uninitDataSize = exe.GetUninitDataSize();

    uint32_t const codeStart = exe.GetCodeAddr();
    uint32_t const initDataStart = exe.GetInitDataAddr();
    // Asumimos que mips buscara el segmento BSS a continuacion de DATA (si
    // existe).
    uint32_t const uninitDataStart = initDataSize > 0 ? initDataStart + initDataSize : codeStart + codeStart;

    // Assert que el segmento TEXT esta al inicio del programa.
    ASSERT(codeStart == 0);

    // Assert de que son contiguos los segmentos TEXT y DATA.
    ASSERT(initDataSize == 0 || initDataStart == codeStart + codeSize);

    uint32_t virtualAddr, segmentOff, writeSize, remaining;

    remaining = codeSize;
    virtualAddr = codeStart;
    segmentOff  = 0;
    while (remaining > 0) {
        uint32_t physicalAddr = TranslateAddress(virtualAddr, pageTable);
        uint32_t offset = virtualAddr % PAGE_SIZE;

        writeSize = min(PAGE_SIZE - offset, remaining);
        exe.ReadCodeBlock(&mainMemory[physicalAddr], writeSize, segmentOff);

        virtualAddr += writeSize;
        segmentOff  += writeSize;
        remaining   -= writeSize;
    }

    remaining = initDataSize;
    virtualAddr = initDataStart;
    segmentOff  = 0;
    while (remaining > 0) {
        uint32_t physicalAddr = TranslateAddress(virtualAddr, pageTable);
        uint32_t offset = physicalAddr % PAGE_SIZE;

        writeSize = min(PAGE_SIZE - offset, remaining);
        exe.ReadDataBlock(&mainMemory[physicalAddr], writeSize, segmentOff);

        virtualAddr += writeSize;
        segmentOff  += writeSize;
        remaining   -= writeSize;
    }

    virtualAddr = uninitDataStart;
    remaining = uninitDataSize;
    while (remaining > 0) {
        uint32_t physicalAddr = TranslateAddress(virtualAddr, pageTable);
        uint32_t offset = virtualAddr % PAGE_SIZE;

        writeSize = min(PAGE_SIZE - offset, remaining);
        memset(&mainMemory[physicalAddr], 0, writeSize);

        virtualAddr += writeSize;
        remaining   -= writeSize;
    }
#endif
}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
    // Las paginas fisicas se liberan en Thread::Exit
    // usando CoreMap::RemoveCurrentThread

    delete [] pageTable;
#ifdef USE_TLB
    delete exe;
    delete swap;
#endif
}

/// Set the initial values for the user-level register set.
///
/// We write these directly into the “machine” registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void
AddressSpace::InitRegisters()
{
    for (unsigned i = 0; i < NUM_TOTAL_REGS; i++) {
        machine->WriteRegister(i, 0);
    }

    // Initial program counter -- must be location of `Start`.
    machine->WriteRegister(PC_REG, 0);

    // Need to also tell MIPS where next instruction is, because of branch
    // delay possibility.
    machine->WriteRegister(NEXT_PC_REG, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we do not
    // accidentally reference off the end!
    machine->WriteRegister(STACK_REG, numPages * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving. When using TLB, all its records are evicted.
void
AddressSpace::SaveState()
{
#ifdef USE_TLB
    for (unsigned i = 0; i < TLB_SIZE; i++) {
        EvictTlb();
    } 
#endif
}

/// On a context switch, restore the machine state so that this address space
/// can run. When using TLB, invalidate all previous records.
void
AddressSpace::RestoreState()
{
#ifdef USE_TLB
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    for (unsigned i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = false;
    }
#else
    machine->GetMMU()->pageTable = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
#endif
}

const TranslationEntry*
AddressSpace::GetTranslationEntry(unsigned virtualPage)
{
    ASSERT(currentThread->space == this);

    if (virtualPage >= numPages) {
        return nullptr;
    }

#ifdef USE_TLB
    if (pageTable[virtualPage].swap) {
        ASSERT(!pageTable[virtualPage].valid);

        unsigned physicalPage = coreMap->FindPhysPage();
        swap->PullSwap(virtualPage, physicalPage);
        DEBUG('x', "SWAPPING IN  VPN=%u ASID=%u\n", virtualPage, asid);

        pageTable[virtualPage].physicalPage = physicalPage;
        pageTable[virtualPage].valid = true;
        pageTable[virtualPage].swap = false;

        coreMap->RegisterPage(virtualPage, physicalPage);
    }
    else if (!pageTable[virtualPage].valid) {
        ASSERT(!pageTable[virtualPage].swap);
        pageTable[virtualPage].valid = true;

        unsigned physicalPage = coreMap->FindPhysPage();
        pageTable[virtualPage].physicalPage = physicalPage; 
        coreMap->RegisterPage(virtualPage, physicalPage);

        uint32_t const virtualStart = virtualPage * PAGE_SIZE;
        uint32_t const virtualEnd = (virtualPage + 1) * PAGE_SIZE;

        uint32_t const codeSize = exe->GetCodeSize();
        uint32_t const virtualCodeStart = exe->GetCodeAddr();
        uint32_t const virtualCodeEnd = virtualCodeStart + codeSize;

        uint32_t const initDataSize = exe->GetInitDataSize();
        uint32_t const virtualInitDataStart = exe->GetInitDataAddr();
        uint32_t const virtualInitDataEnd = virtualInitDataStart + initDataSize;

        uint32_t const uninitDataSize = exe->GetUninitDataSize();
        uint32_t const virtualUninitDataStart = initDataSize > 0 ? virtualInitDataEnd : virtualCodeEnd;
        uint32_t const virtualUninitDataEnd = virtualUninitDataStart + uninitDataSize;

        char *mainMemory = machine->GetMMU()->mainMemory;

        ASSERT(codeSize > 0); 

        // code: cargar codigo
        if (virtualStart <= virtualCodeEnd && virtualEnd >= virtualCodeStart) {
            uint32_t virtualCopyStart = max(virtualCodeStart, virtualStart);
            uint32_t virtualCopyEnd = min(virtualEnd, virtualCodeEnd);

            uint32_t writeSize = virtualCopyEnd - virtualCopyStart;
            uint32_t segmentOff = virtualCopyStart - virtualCodeStart;

            uint32_t physicalAddr = TranslateAddress(virtualCopyStart, pageTable);

            exe->ReadCodeBlock(&mainMemory[physicalAddr], writeSize, segmentOff);
            pageTable[virtualPage].readOnly = true;
        }

        // data: cargar data
        if (initDataSize > 0 && virtualStart <= virtualInitDataEnd && virtualEnd >= virtualInitDataStart) {
            uint32_t virtualCopyStart = max(virtualInitDataStart, virtualStart);
            uint32_t virtualCopyEnd = min(virtualEnd, virtualInitDataEnd);

            uint32_t writeSize = virtualCopyEnd - virtualCopyStart;
            uint32_t segmentOff = virtualCopyStart - virtualInitDataStart;

            uint32_t physicalAddr = TranslateAddress(virtualCopyStart, pageTable);

            exe->ReadDataBlock(&mainMemory[physicalAddr], writeSize, segmentOff);
            pageTable[virtualPage].readOnly = false;
        }

        // bss: cargar cero
        if (uninitDataSize > 0 && virtualStart <= virtualUninitDataEnd && virtualEnd >= virtualUninitDataStart) {
            uint32_t virtualCopyStart = max(virtualUninitDataStart, virtualStart);
            uint32_t virtualCopyEnd = min(virtualEnd, virtualUninitDataEnd);

            uint32_t writeSize = virtualCopyEnd - virtualCopyStart;

            uint32_t physicalAddr = TranslateAddress(virtualCopyStart, pageTable);

            memset(&mainMemory[physicalAddr], 0, writeSize);
            pageTable[virtualPage].readOnly = false;
        }
    }
    // Pagina valida en el swap.

#endif

    return &pageTable[virtualPage];
}

#ifdef USE_TLB
unsigned
AddressSpace::EvictTlb() {
    ASSERT(currentThread->space == this);

    TranslationEntry *victim = &machine->GetMMU()->tlb[tlbVictim];
    if (victim->valid) {
        pageTable[victim->virtualPage] = *victim;
        victim->valid = false;
    }

    /// Increment tlbVictim.
    unsigned _tlbVictim = tlbVictim;
    tlbVictim = (tlbVictim + 1) % TLB_SIZE;
    return _tlbVictim;
}

void
AddressSpace::SwapPage(unsigned vpn) {
    DEBUG('x', "SWAPPING OUT VPN=%u ASID=%u\n", vpn, asid);

    ASSERT(pageTable[vpn].valid);
    ASSERT(!pageTable[vpn].swap);

    for (unsigned i = 0; i < TLB_SIZE; i++) {
        TranslationEntry *entry = &machine->GetMMU()->tlb[i];
        if (entry->valid && entry->virtualPage == vpn) {
            entry->valid = false;
            entry->swap = !entry->readOnly;
        }
    }    

    pageTable[vpn].valid = false;
    pageTable[vpn].swap = !pageTable[vpn].readOnly;

    if (pageTable[vpn].swap) {
        unsigned ppn = pageTable[vpn].physicalPage;
        swap->WriteSwap(vpn, ppn);
    }
}
#endif

unsigned
AddressSpace::GetASid() {
    return asid;
}
