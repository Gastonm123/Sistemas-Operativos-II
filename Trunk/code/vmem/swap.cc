#include "swap.hh"
#include "threads/system.hh"

SWAP::SWAP(unsigned id) {
    sprintf(name, "SWAP.%u", id);
    fileSystem->Create(name, 0);
    swapFile = fileSystem->Open(name);
}

SWAP::~SWAP() {
    delete swapFile;
    fileSystem->Remove(name);
}

void
SWAP::WriteSWAP(unsigned vpn, unsigned ppn) {
    char *mainMemory = machine->GetMMU()->mainMemory;
    unsigned offset = ppn * PAGE_SIZE;
    char *physicalAddress = mainMemory + offset;
    
    unsigned filePosition = vpn * PAGE_SIZE;
    swapFile->WriteAt(physicalAddress, PAGE_SIZE, filePosition);
}

void
SWAP::PullSWAP(unsigned vpn, unsigned ppn) {
    char *mainMemory = machine->GetMMU()->mainMemory;
    unsigned offset = ppn * PAGE_SIZE;
    char *physicalAddress = mainMemory + offset;
    
    unsigned filePosition = vpn * PAGE_SIZE;
    swapFile->ReadAt(physicalAddress, PAGE_SIZE, filePosition);
}
