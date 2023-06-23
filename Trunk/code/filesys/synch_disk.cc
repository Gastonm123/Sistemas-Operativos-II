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
    lock = new Lock("synch disk lock");
    disk = new Disk(name, DiskRequestDone, this);
    cache = new DiskCache[CACHE_SIZE];
    writeQ = new List<DiskCache*>;
    memset(cache, 0, sizeof(DiskCache) * CACHE_SIZE);
    victim = 0;
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

/// Find an entry that is suitable to be overwritten by some other data.
/// If the entry is dirty, write it to disk before returning.
unsigned
SynchDisk::ReclaimCache()
{
    for (unsigned loop = 0; loop < 4; loop++) {
        for (unsigned i = 0; i < CACHE_SIZE; i++) {
            DiskCache *victimEntry = &cache[victim];

            bool suitable = false;
            switch (loop) {
                case 0:
                    suitable = (!victimEntry->use &&
                                !victimEntry->dirty);
                break;
                case 1:
                    /// On the second loop if an entry is unused it will be
                    /// dirty.
                    suitable = (!victimEntry->use);
                    victimEntry->use = 0;
                break;
                case 2:
                    suitable = (!victimEntry->dirty);
                break;
                default:
                    suitable = true;
                break;
            }

            if (suitable) {
                if (victimEntry->dirty) { // Take one write operation from the
                                          // queue and switch places with victim.
                    DiskCache *write = writeQ->Pop();

                    lock->Acquire(); // Only one disk I/O at a time.
                    disk->WriteRequest(write->sector, write->data);
                    semaphore->P();   // wait for interrupt
                    lock->Release();

                    if (write != victimEntry) {
                        memcpy(write, victimEntry, sizeof(DiskCache));
                    }
                    victimEntry->dirty = false;
                }

                unsigned _victim = victim;
                victim = (victim + 1) % CACHE_SIZE;
                return _victim;
            }

            victim = (victim + 1) % CACHE_SIZE;
        }
    }
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

    for (unsigned i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].sector == sectorNumber) {
            memcpy(data, cache[i].data, SECTOR_SIZE);
            cache[i].use = true;
            return;
        }
    }

    lock->Acquire();  // Only one disk I/O at a time.
    disk->ReadRequest(sectorNumber, data);
    semaphore->P();   // Wait for interrupt.
    lock->Release();

    unsigned entry = ReclaimCache();
    cache[entry].sector = sectorNumber;
    cache[entry].use = true;
    memcpy(cache[entry].data, data, SECTOR_SIZE);

    // Read ahead.
    // Cache the next block.
    if ((unsigned) sectorNumber < NUM_SECTORS-1) {
        char *next = new char[SECTOR_SIZE];

        lock->Acquire();
        disk->ReadRequest(sectorNumber+1, next);
        semaphore->P();   // Wait for interrupt.
        lock->Release();

        entry = ReclaimCache();
        cache[entry].sector = sectorNumber+1;
        cache[entry].use = true;
        memcpy(cache[entry].data, next, SECTOR_SIZE);
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

    for (unsigned i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].sector == sectorNumber) {
            memcpy(cache[i].data, data, SECTOR_SIZE);
            cache[i].use = true;
            cache[i].dirty = true;
            return;
        }
    }

    /// Write-behind.
    /// Cache data and append to writeQ.
    unsigned entry = ReclaimCache();
    cache[entry].sector = sectorNumber;
    cache[entry].use = true;
    cache[entry].dirty = true;
    memcpy(cache[entry].data, data, SECTOR_SIZE);
    writeQ->Append(&cache[entry]);
}

/// Disk interrupt handler.  Wake up any thread waiting for the disk
/// request to finish.
void
SynchDisk::RequestDone()
{
    semaphore->V();
}
