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


/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file)
{
    ASSERT(executable_file != nullptr);

    Executable exe (executable_file);
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
          // If the code segment was entirely on a separate page, we could
          // set its pages to be read-only.
    }

    char *mainMemory = machine->GetMMU()->mainMemory;

    // Then, copy in the code and data segments into memory.
    uint32_t codeSize = exe.GetCodeSize();
    uint32_t initDataSize = exe.GetInitDataSize();
    uint32_t uninitDataSize = exe.GetUninitDataSize();

    uint32_t virtualAddr, virtualPage, offset, segmentOff, physicalPage,
             physicalAddr, writeSize;

    // Assert que el segmento TEXT esta al inicio del programa.
    ASSERT(exe.GetCodeAddr() == 0);

    virtualAddr = exe.GetCodeAddr();
    virtualPage = virtualAddr / PAGE_SIZE;
    offset      = virtualAddr % PAGE_SIZE;
    segmentOff  = 0;
    for (; codeSize > 0; virtualPage++) {
        physicalPage = pageTable[virtualPage].physicalPage;
        physicalAddr = physicalPage * PAGE_SIZE + offset;

        //DEBUG('a', "Initializing code segment, at 0x%X, size %u\n",
        //      virtualAddr, codeSize);

        writeSize = min(PAGE_SIZE - offset, codeSize);
        exe.ReadCodeBlock(&mainMemory[physicalAddr], writeSize, segmentOff);

        segmentOff += writeSize;
        codeSize   -= writeSize;
        offset      = 0;
    }

    // Assert de que son contiguos los segmentos TEXT y DATA.
    ASSERT(initDataSize == 0 || exe.GetInitDataAddr() == virtualAddr +
           exe.GetCodeSize());

    virtualAddr = exe.GetInitDataAddr();
    virtualPage = virtualAddr / PAGE_SIZE;
    offset      = virtualAddr % PAGE_SIZE;
    segmentOff  = 0;
    for (; initDataSize > 0; virtualPage++) {
        physicalPage = pageTable[virtualPage].physicalPage;
        physicalAddr = physicalPage * PAGE_SIZE + offset;

        //DEBUG('a', "Initializing data segment, at 0x%X, size %u\n",

        writeSize = min(PAGE_SIZE - offset, initDataSize);
        exe.ReadDataBlock(&mainMemory[physicalAddr], writeSize, segmentOff);

        segmentOff   += writeSize;
        initDataSize -= writeSize;
        offset        = 0;
    }

    // Asumimos que mips buscara el segmento BSS a continuacion de DATA (si
    // existe).
    if (initDataSize > 0) {
        virtualAddr = exe.GetInitDataAddr() + exe.GetInitDataSize();
    }
    else {
        virtualAddr = exe.GetCodeAddr() + exe.GetCodeSize();
    }
    virtualPage = virtualAddr / PAGE_SIZE;
    offset      = virtualAddr % PAGE_SIZE;
    for (; uninitDataSize > 0; virtualPage++) {
        physicalPage = pageTable[virtualPage].physicalPage;
        physicalAddr = physicalPage * PAGE_SIZE + offset;

        //DEBUG('a', "Initializing data segment, at 0x%X, size %u\n",

        writeSize = min(PAGE_SIZE - offset, uninitDataSize);
        memset(&mainMemory[physicalAddr], 0, writeSize);

        uninitDataSize -= writeSize;
        offset          = 0;
    }
}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
    for (unsigned i = 0; i < numPages; i++) {
        physPages->Clear(pageTable[i].physicalPage);
    }
    delete [] pageTable;
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
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
    machine->GetMMU()->pageTable     = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
}
