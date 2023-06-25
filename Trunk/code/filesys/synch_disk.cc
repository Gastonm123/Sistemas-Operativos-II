/// Routines to synchronously access the disk.  The physical disk is an
/// asynchronous device (disk requests return immediately, and an interrupt
/// happens later on).  This is a layer on top of the disk providing a
/// synchronous interface (requests wait until the request completes).
///
/// Use a semaphore to synchronize the interrupt handlers with the pending
/// requests.  And, because the physical disk can only handle one operation
/// at a time, use a lock to enforce mutual exclusion.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "synch_disk.hh"
#include <string.h>

const unsigned CACHE_SIZE = 64;
const unsigned WRITEQ_SIZE = 32; //< The maximum number of deferred write requests.

/// Disk interrupt handler.  Need this to be a C routine, because C++ cannot
/// handle pointers to member functions.
static void
DiskRequestDone(void *arg)
{
    ASSERT(arg != nullptr);
    SynchDisk *disk = (SynchDisk *) arg;
    disk->RequestDone();
}

/// Initialize the synchronous interface to the physical disk, in turn
/// initializing the physical disk.
///
/// * `name` is a UNIX file name to be used as storage for the disk data
///   (usually, `DISK`).
SynchDisk::SynchDisk(const char *name)
{
    semaphore = new Semaphore("synch disk", 0);
    lock      = new Lock("synch disk lock");
    disk      = new Disk(name, DiskRequestDone, this);
    cache     = new DiskCache[CACHE_SIZE];
    writeQ    = new List<DiskCache*>;
    memset(cache, 0, sizeof(DiskCache) * CACHE_SIZE);
    victim = 0;
    numCachedWrites = 0;
}

/// De-allocate data structures needed for the synchronous disk abstraction.
SynchDisk::~SynchDisk()
{
    delete disk;
    delete lock;
    delete semaphore;
    delete [] cache;
    delete writeQ;
}

/// Flush all cached writes.
void
SynchDisk::FlushCache()
{
    lock->Acquire();
    while (!writeQ->IsEmpty()) {
        DiskCache *write = writeQ->Pop();
        numCachedWrites--;
        disk->WriteRequest(write->sector, write->data);
        semaphore->P();
        write->valid = false;
        write->use   = false;
        write->dirty = false;
    }
    lock->Release();
}

#include <ctype.h>

void
SynchDisk::PrintCache()
{
    lock->Acquire();
    printf("Cache contents:\n");
    for (unsigned i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid) {
            printf("    sector: %d, dirty: %d\n", cache[i].sector, cache[i].dirty);
            printf("    sector contents:\n");
            char *data = cache[i].data;
            for (unsigned j = 0; j < SECTOR_SIZE; j++) {
                if (isprint(data[j])) {
                    printf("%c", data[j]);
                } else {
                    printf("\\%X", (unsigned char) data[j]);
                }
            }
            printf("\n");
        }
    }
    lock->Release();
}

/// Find an entry that is suitable to be overwritten by some other data.
/// NOTICE: esta funcion no es reentrante asi que debe tomarse el lock antes de llamarla.
unsigned
SynchDisk::ReclaimCache()
{
    if (numCachedWrites > WRITEQ_SIZE) {
        DiskCache *write = writeQ->Pop();
        numCachedWrites--;
        disk->WriteRequest(write->sector, write->data);
        semaphore->P();
        write->dirty = false;
        return (write - cache);
    }

    /// Ignore dirty pages and use second chance.
    for (unsigned i = 0; i < 2*CACHE_SIZE; i++) {
        DiskCache *victimEntry = &cache[victim];
        if (!victimEntry->valid || (!victimEntry->use && !victimEntry->dirty)) {
            unsigned _victim = victim;
            victim = (victim + 1) % CACHE_SIZE;
            return _victim;
        }
        victimEntry->use = false;
        victim = (victim + 1) % CACHE_SIZE;
    }

    ASSERT(false);
    return 0;
}

/// Read the contents of a disk sector into a buffer.  Return only after the
/// data has been read.
///
/// * `sectorNumber` is the disk sector to read.
/// * `data` is the buffer to hold the contents of the disk sector.
void
SynchDisk::ReadSector(int sectorNumber, char *data)
{
    ASSERT(data != nullptr);

    lock->Acquire();
    bool nextIsCached = false;
    for (unsigned i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid) {
            if (cache[i].sector == sectorNumber) {
                memcpy(data, cache[i].data, SECTOR_SIZE);
                cache[i].use = true;
                lock->Release();
                return;
            }
            if (cache[i].sector == sectorNumber+1) {
                nextIsCached = true;
            }
        }
    }

    char *next = nullptr;
    disk->ReadRequest(sectorNumber, data);
    semaphore->P();   // Wait for interrupt.
    if ((unsigned) sectorNumber < NUM_SECTORS-1 && !nextIsCached) {
        next = new char[SECTOR_SIZE];
        disk->ReadRequest(sectorNumber+1, next); // Read ahead.
        semaphore->P();   // Wait for interrupt.
    }

    unsigned entry = ReclaimCache();
    cache[entry].sector = sectorNumber;
    cache[entry].use = true;
    cache[entry].valid = true;
    memcpy(cache[entry].data, data, SECTOR_SIZE);

    // Cache the next block.
    if (next) {
        entry = ReclaimCache();
        cache[entry].sector = sectorNumber+1;
        cache[entry].use = true;
        cache[entry].valid = true;
        memcpy(cache[entry].data, next, SECTOR_SIZE);
        delete next;
    }
    lock->Release();
}

/// Write the contents of a buffer into a disk sector.  Return only
/// after the data has been written.
///
/// * `sectorNumber` is the disk sector to be written.
/// * `data` are the new contents of the disk sector.
void
SynchDisk::WriteSector(int sectorNumber, const char *data)
{
    ASSERT(data != nullptr);

    lock->Acquire();
    for (unsigned i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid && cache[i].sector == sectorNumber) {
            if (!cache[i].dirty) {
                writeQ->Append(&cache[i]);
                numCachedWrites++;
                cache[i].dirty = true;
            }
            memcpy(cache[i].data, data, SECTOR_SIZE);
            cache[i].use = true;
            lock->Release();
            return;
        }
    }

    /// Write-behind.
    /// Cache data and append to writeQ.
    unsigned entry = ReclaimCache();
    cache[entry].sector = sectorNumber;
    cache[entry].use = true;
    cache[entry].dirty = true;
    cache[entry].valid = true;
    memcpy(cache[entry].data, data, SECTOR_SIZE);
    writeQ->Append(&cache[entry]);
    numCachedWrites++;
    lock->Release();
}

/// Disk interrupt handler.  Wake up any thread waiting for the disk
/// request to finish.
void
SynchDisk::RequestDone()
{
    semaphore->V();
}
