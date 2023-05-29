#include "swap.hh"
#include "threads/system.hh"

SWAP::SWAP() {
    unsigned tid = currentThread->GetTid();
    sprintf(name, "SWAP.%u", tid); 
    fileSystem->Create(name, 0);
    swapFile = fileSystem->Open(name);
}

SWAP::~SWAP() {
    delete swapFile;
    fileSystem->Remove(name);
}

void
SWAP::WriteSWAP(unsigned vpn, unsigned ppn) {
    // Lee directamente la memoria principal
    // TODO: deberia hacerlo la MMU?
    char* mainMemory = machine->GetMMU()->mainMemory;
    unsigned offset = ppn * PAGE_SIZE;
    char *physicalAddress = mainMemory + offset;
    
    unsigned filePosition = vpn * PAGE_SIZE;
    swapFile->WriteAt(physicalAddress, PAGE_SIZE, filePosition);
}

void
SWAP::PullSWAP(unsigned vpn, unsigned ppn) {
    // Escribe directamente la memoria principal
    // TODO: deberia hacerlo la MMU?
    char* mainMemory = machine->GetMMU()->mainMemory;
    unsigned offset = ppn * PAGE_SIZE;
    char *physicalAddress = mainMemory + offset;
    
    unsigned filePosition = vpn * PAGE_SIZE;
    swapFile->ReadAt(physicalAddress, PAGE_SIZE, filePosition);
}
