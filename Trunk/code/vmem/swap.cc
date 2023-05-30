#include "swap.hh"
#include "threads/system.hh"

Swap::Swap(unsigned id) {
    sprintf(name, "swap.%u", id);
    fileSystem->Create(name, 0);
    swapFile = fileSystem->Open(name);
}

Swap::~Swap() {
    delete swapFile;
    fileSystem->Remove(name);
}

void
Swap::WriteSwap(unsigned vpn, unsigned ppn) {
    char *mainMemory = machine->GetMMU()->mainMemory;
    unsigned offset = ppn * PAGE_SIZE;
    char *physicalAddress = mainMemory + offset;
    
    unsigned filePosition = vpn * PAGE_SIZE;
    swapFile->WriteAt(physicalAddress, PAGE_SIZE, filePosition);
}

void
Swap::PullSwap(unsigned vpn, unsigned ppn) {
    char *mainMemory = machine->GetMMU()->mainMemory;
    unsigned offset = ppn * PAGE_SIZE;
    char *physicalAddress = mainMemory + offset;
    
    unsigned filePosition = vpn * PAGE_SIZE;
    swapFile->ReadAt(physicalAddress, PAGE_SIZE, filePosition);
}
