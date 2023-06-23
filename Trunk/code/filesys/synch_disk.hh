/// Data structures to export a synchronous interface to the raw disk device.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_FILESYS_SYNCHDISK__HH
#define NACHOS_FILESYS_SYNCHDISK__HH


#include "machine/disk.hh"
#include "threads/lock.hh"
#include "threads/semaphore.hh"
#include "lib/list.hh"

///< This class defines a cache entry for the synchronous disk.
///< Cache entries somewhat like ram pages, making reclaim a task that can be
///< performed using second chance.
class DiskCache {
public:
    ////<  Sector number.
    int sector;

    ///< Flag signaling this sector is to be written at some later time.
    bool dirty;

    ///< Flag signaling this entry is valid.
    bool use;

    ///< Sector data.
    char data[SECTOR_SIZE];
};

/// The following class defines a "synchronous" disk abstraction.
///
/// As with other I/O devices, the raw physical disk is an asynchronous
/// device -- requests to read or write portions of the disk return
/// immediately, and an interrupt occurs later to signal that the operation
/// completed.  (Also, the physical characteristics of the disk device assume
/// that only one operation can be requested at a time).
///
/// This class provides the abstraction that for any individual thread making
/// a request, it waits around until the operation finishes before returning.
class SynchDisk {
public:

    /// Initialize a synchronous disk, by initializing the raw Disk.
    SynchDisk(const char *name);

    /// De-allocate the synch disk data.
    ~SynchDisk();

    /// Read/write a disk sector, returning only once the data is actually
    /// read or written.  These call `Disk::ReadRequest`/`WriteRequest` and
    /// then wait until the request is done.

    void ReadSector(int sectorNumber, char *data);
    void WriteSector(int sectorNumber, const char *data);

    /// Called by the disk device interrupt handler, to signal that the
    /// current disk operation is complete.
    void RequestDone();

private:
    Disk *disk;  ///< Raw disk device.
    Semaphore *semaphore;  ///< To synchronize requesting thread with the
                           ///< interrupt handler.
    Lock *lock;  ///< Only one read/write request can be sent to the disk at
                 ///< a time.
    DiskCache *cache; ///< Read and write cache.
    unsigned victim; ///< Reclaim victim.

    /// Find an entry that is suitable to be overwritten by some other data.
    /// If the entry is dirty, write it to disk before returning.
    unsigned ReclaimCache();

    List<DiskCache*> *writeQ; ///< Writes must be processed in order.
};


#endif
