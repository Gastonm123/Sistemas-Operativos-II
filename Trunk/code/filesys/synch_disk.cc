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
    cacheLock = new Lock("synch disk cache lock");
    memset(cache, 0, sizeof(DiskCache) * CACHE_SIZE);
    victim = 0;
    numCachedWrites = 0;
}

/// Flush all cached writes.
void
SynchDisk::FlushCache()
{
    while (!writeQ->IsEmpty()) {
        DiskCache *write = writeQ->Pop();
        numCachedWrites--;
        lock->Acquire();
        disk->WriteRequest(write->sector, write->data);
        semaphore->P();
        lock->Release();
        write->valid = false;
        write->use   = false;
        write->dirty = false;
    }
}

#include <ctype.h>

void
SynchDisk::PrintCache()
{
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
}

/// De-allocate data structures needed for the synchronous disk abstraction.
SynchDisk::~SynchDisk()
{
    delete disk;
    delete lock;
    delete semaphore;
    delete [] cache;
    delete writeQ;
    delete cacheLock;
}

/// Find an entry that is suitable to be overwritten by some other data.
unsigned
SynchDisk::ReclaimCache()
{
    if (numCachedWrites > WRITEQ_SIZE) {
        DiskCache *write = writeQ->Pop();
        numCachedWrites--;
        lock->Acquire();
        disk->WriteRequest(write->sector, write->data);
        semaphore->P();
        lock->Release();
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

    bool nextIsCached = false;
    cacheLock->Acquire();
    for (unsigned i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid) {
            if (cache[i].sector == sectorNumber) {
                memcpy(data, cache[i].data, SECTOR_SIZE);
                cache[i].use = true;
                cacheLock->Release();
                return;
            }
            if (cache[i].sector == sectorNumber+1) {
                nextIsCached = true;
            }
        }
    }
    cacheLock->Release();

    lock->Acquire();  // Only one disk I/O at a time.
    disk->ReadRequest(sectorNumber, data);
    semaphore->P();   // Wait for interrupt.
    lock->Release();

    cacheLock->Acquire();
    unsigned entry = ReclaimCache();
    cache[entry].sector = sectorNumber;
    cache[entry].use = true;
    cache[entry].valid = true;
    memcpy(cache[entry].data, data, SECTOR_SIZE);
    cacheLock->Release();

    // Read ahead.
    // Cache the next block.
    if ((unsigned) sectorNumber < NUM_SECTORS-1 && !nextIsCached) {
        char *next = new char[SECTOR_SIZE];

        lock->Acquire();
        disk->ReadRequest(sectorNumber+1, next);
        semaphore->P();   // Wait for interrupt.
        lock->Release();

        cacheLock->Acquire();
        entry = ReclaimCache();
        cache[entry].sector = sectorNumber+1;
        cache[entry].use = true;
        cache[entry].valid = true;
        memcpy(cache[entry].data, next, SECTOR_SIZE);
        cacheLock->Release();
        delete next;
    }
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

    cacheLock->Acquire();
    for (unsigned i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid && cache[i].sector == sectorNumber) {
            if (!cache[i].dirty) {
                writeQ->Append(&cache[i]);
                numCachedWrites++;
                cache[i].dirty = true;
            }
            memcpy(cache[i].data, data, SECTOR_SIZE);
            cache[i].use = true;
            cacheLock->Release();
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
    cacheLock->Release();
}

/// Disk interrupt handler.  Wake up any thread waiting for the disk
/// request to finish.
void
SynchDisk::RequestDone()
{
    semaphore->V();
}
