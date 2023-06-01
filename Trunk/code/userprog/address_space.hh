/// Data structures to keep track of executing user programs (address
/// spaces).
///
/// For now, we do not keep any information about address spaces.  The user
/// level CPU state is saved and restored in the thread executing the user
/// program (see `thread.hh`).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_USERPROG_ADDRESSSPACE__HH
#define NACHOS_USERPROG_ADDRESSSPACE__HH


#include "filesys/file_system.hh"
#include "machine/translation_entry.hh"


const unsigned USER_STACK_SIZE = 1024;  ///< Increase this as necessary!

class Executable;
class Swap;

#ifdef USE_TLB
class PageTableEntry {
public:
    TranslationEntry base;
    bool swap;
};
#endif

class AddressSpace {
public:

    /// Create an address space to run a user program.
    ///
    /// The address space is initialized from an already opened file.
    /// The program contained in the file is loaded into memory and
    /// everything is set up so that user instructions can start to be
    /// executed.
    ///
    /// Parameters:
    /// * `executableFile` is the open file that corresponds to the
    ///   program; it contains the object code to load into memory.
    /// * `asid` es el address space id.
    AddressSpace(OpenFile *executableFile, unsigned asid);

    /// De-allocate an address space.
    ~AddressSpace();

    /// Initialize user-level CPU registers, before jumping to user code.
    void InitRegisters();

    /// Save/restore address space-specific info on a context switch.

    void SaveState();
    void RestoreState();

    /// Returns a pointer to the translation entry associated with the
    /// given page, or nullptr if it is outside of the virtual address space.
    ///
    /// * `virtualPage` is the requested page.
    const TranslationEntry* GetTranslationEntry(unsigned virtualPage);

#ifdef USE_TLB
    /// Evict an entry from the machine TLB and save its metadata into the page
    /// table. Return the index evicted TLB entry.
    unsigned EvictTlb();

    /// Move page to swap file; returns physical page.
    /// * `vpn` is the virtual page number of the victim page.
    void SwapPage(unsigned vpn);

    /// Look into TLB and update page table.
    void UpdatePageTable();

    /// Get `use` bit of given virtual page.
    /// * `vpn` is the virtual page number.
    bool UseBit(unsigned vpn);

    /// Get `dirty` bit of given virtual page.
    /// * `vpn` is the virtual page number.
    bool DirtyBit(unsigned vpn);

    /// Clear `use` bit of given virtual page.
    /// Used in page replacement.
    /// `vpn` is the virtual page number.
    void ClearUseBit(unsigned vpn);
#endif

    /// Returns address space id.
    unsigned GetASid();

private:

#ifdef USE_TLB
    PageTableEntry *pageTable;
#else
    TranslationEntry *pageTable;
#endif

    /// Number of pages in the virtual address space.
    unsigned numPages;

    /// Address space id (actualmente igual al tid).
    unsigned asid;
#ifdef USE_TLB
    /// Executable of the program file.
    Executable *exe;

    /// Next tlb victim.
    unsigned tlbVictim;

    Swap *swap;
#endif

};


#endif
