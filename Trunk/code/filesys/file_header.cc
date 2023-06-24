/// Routines for managing the disk file header (in UNIX, this would be called
/// the i-node).
///
/// The file header is used to locate where on disk the file's data is
/// stored.  We implement this as a fixed size table of pointers -- each
/// entry in the table points to the disk sector containing that portion of
/// the file data (in other words, there are no indirect or doubly indirect
/// blocks). The table size is chosen so that the file header will be just
/// big enough to fit in one disk sector,
///
/// Unlike in a real system, we do not keep track of file permissions,
/// ownership, last modification date, etc., in the file header.
///
/// A file header can be initialized in two ways:
///
/// * for a new file, by modifying the in-memory data structure to point to
///   the newly allocated data blocks;
/// * for a file already on disk, by reading the file header from disk.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_header.hh"
#include "threads/system.hh"

#include <ctype.h>
#include <stdio.h>

/// Initialize a fresh file header for a newly created file.  Allocate data
/// blocks for the file out of the map of free disk blocks.  Return false if
/// there are not enough free blocks to accomodate the new file.
///
/// * `freeMap` is the bit map of free disk sectors.
/// * `fileSize` is the bit map of free disk sectors.
bool
FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize, bool directory)
{
    ASSERT(freeMap != nullptr);

    if (fileSize > MAX_FILE_SIZE) {
        return false;
    }

    raw.numBytes = fileSize;
    raw.numSectors = DivRoundUp(fileSize, SECTOR_SIZE);

    /// The header sector is reserved beforehand, but the sectors containing
    /// indirect blocks are not.
    unsigned numIndirect = 0; //< Sectors containing indirect blocks.

    if (raw.numSectors > NUM_DIRECT) {
        numIndirect += 1;
    }
    if (raw.numSectors > NUM_DIRECT + NUM_DATAPTR) {
        numIndirect += DivRoundUp(raw.numSectors - NUM_DIRECT - NUM_DATAPTR,
                                  NUM_DATAPTR) + 1;
    }

    if (freeMap->CountClear() < raw.numSectors + numIndirect) {
        return false;  // Not enough space.
    }

    /// Reserve indirect blocks first.
    unsigned *dataPtrList = nullptr;
    for (unsigned i = 0; i < numIndirect; i++) {
        switch (i) {
        case 0:
            raw.dataPtr = freeMap->Find();
        break;
        case 1:
            raw.dataPtrPtr = freeMap->Find();
            dataPtrList = new unsigned[NUM_DATAPTR];
        break;
        default:
            dataPtrList[i-2] = freeMap->Find();
        }
    }
    if (dataPtrList) {
        synchDisk->WriteSector(raw.dataPtrPtr, (char*) dataPtrList);
        // delete [] dataPtrList; the list is reused.
    }

    /// Reserve data blocks.
    unsigned remaining = raw.numSectors;
    for (unsigned i = 0; remaining > 0 && i < NUM_DIRECT; i++) {
        raw.dataSectors[i] = freeMap->Find();
        remaining--;
    }

    if (remaining) {
        unsigned *dataPtrBuf = new unsigned[NUM_DATAPTR];
        for (unsigned i = 0; remaining > 0 && i < NUM_DATAPTR; i++) {
            dataPtrBuf[i] = freeMap->Find();
            remaining--;
        }
        synchDisk->WriteSector(raw.dataPtr, (char*) dataPtrBuf);
        delete [] dataPtrBuf;
    }

    if (remaining) {
        unsigned *dataPtrBuf  = new unsigned[NUM_DATAPTR]; //< reused for every block in
                                                           //< dataPtrList.
        unsigned sector = 0; //< which sector of dataPtrList.
        unsigned c = 0;      //< contents in dataPtrBuf.
        for (; remaining > 0; remaining--) {
            dataPtrBuf[c] = freeMap->Find();

            if (++c == NUM_DATAPTR) {
                synchDisk->WriteSector(dataPtrList[sector], (char*) dataPtrBuf);
                sector++;
                c = 0;
            }
        }
        if (c > 0) {
            synchDisk->WriteSector(dataPtrList[sector], (char*) dataPtrBuf);
        }
        delete [] dataPtrBuf;
        delete [] dataPtrList;
    }

    raw.directory = directory;
    return true;
}

/// De-allocate all the space allocated for data blocks for this file.
///
/// * `freeMap` is the bit map of free disk sectors.
void
FileHeader::Deallocate(Bitmap *freeMap)
{
    ASSERT(freeMap != nullptr);

    unsigned numIndirect = 0; //< Sectors containing indirect blocks.

    if (raw.numSectors > NUM_DIRECT) {
        numIndirect += 1;
    }
    if (raw.numSectors > NUM_DIRECT + NUM_DATAPTR) {
        numIndirect += DivRoundUp(raw.numSectors - NUM_DIRECT - NUM_DATAPTR,
                                  NUM_DATAPTR) + 1;
    }

    /// Deallocate indirect blocks first.
    unsigned *dataPtrList = nullptr;
    /// dataPtrList is not freed as it is used later.
    for (unsigned i = 0; i < numIndirect; i++) {
        switch (i) {
        case 0:
            ASSERT(freeMap->Test(raw.dataPtr));  // ought to be marked!
            freeMap->Clear(raw.dataPtr);
        break;
        case 1:
            ASSERT(freeMap->Test(raw.dataPtrPtr));  // ought to be marked!
            freeMap->Clear(raw.dataPtrPtr);

            dataPtrList = new unsigned[NUM_DATAPTR];
            synchDisk->ReadSector(raw.dataPtrPtr, (char*) dataPtrList);
        break;
        default:
            ASSERT(freeMap->Test(dataPtrList[i-2]));  // ought to be marked!
            freeMap->Clear(dataPtrList[i-2]);
        }
    }

    /// Deallocate data blocks.
    unsigned remaining = raw.numSectors;

    for (unsigned i = 0; remaining > 0 && i < NUM_DIRECT; i++) {
        ASSERT(freeMap->Test(raw.dataSectors[i]));  // ought to be marked!
        freeMap->Clear(raw.dataSectors[i]);
        remaining--;
    }

    if (remaining) {
        unsigned *dataPtrBuf = new unsigned[NUM_DATAPTR];
        synchDisk->ReadSector(raw.dataPtr, (char*) dataPtrBuf);
        for (unsigned i = 0; remaining > 0 && i < NUM_DATAPTR; i++) {
            ASSERT(freeMap->Test(dataPtrBuf[i]));  // ought to be marked!
            freeMap->Clear(dataPtrBuf[i]);
            remaining--;
        }
        delete [] dataPtrBuf;
    }

    if (remaining) {
        unsigned *dataPtrBuf  = new unsigned[NUM_DATAPTR]; //< reused for every block in
                                                           //< dataPtrList.
        unsigned sector = 0; //< which sector of dataPtrList.
        unsigned c = 0;      //< position in dataPtrBuf.
        synchDisk->ReadSector(dataPtrList[sector], (char*) dataPtrBuf);
        for (; remaining > 0; remaining--) {
            ASSERT(freeMap->Test(dataPtrBuf[c]));  // ought to be marked!
            freeMap->Clear(dataPtrBuf[c]);

            if (++c == NUM_DATAPTR) {
                synchDisk->ReadSector(dataPtrList[++sector], (char*) dataPtrBuf);
                c = 0;
            }
        }
        delete [] dataPtrBuf;
        delete [] dataPtrList;
    }
}

/// Fetch contents of file header from disk.
///
/// * `sector` is the disk sector containing the file header.
void
FileHeader::FetchFrom(unsigned sector)
{
    synchDisk->ReadSector(sector, (char *) &raw);
}

/// Write the modified contents of the file header back to disk.
///
/// * `sector` is the disk sector to contain the file header.
void
FileHeader::WriteBack(unsigned sector)
{
    synchDisk->WriteSector(sector, (char *) &raw);
}

/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.
unsigned
FileHeader::ByteToSector(unsigned offset)
{
    unsigned virtualSector = offset / SECTOR_SIZE;
    if (virtualSector < NUM_DIRECT) {
        return raw.dataSectors[virtualSector];
    }

    if (virtualSector < NUM_DIRECT + NUM_DATAPTR) {
        unsigned *dataPtrBuf = new unsigned[NUM_DATAPTR];
        synchDisk->ReadSector(raw.dataPtr, (char*) dataPtrBuf);
        unsigned sector = dataPtrBuf[virtualSector - NUM_DIRECT];
        delete [] dataPtrBuf;
        return sector;
    }

    unsigned *dataPtrBuf  = new unsigned[NUM_DATAPTR];
    unsigned *dataPtrList = new unsigned[NUM_DATAPTR];
    virtualSector  = virtualSector - NUM_DIRECT - NUM_DATAPTR;
    unsigned index = virtualSector / NUM_DATAPTR;
    synchDisk->ReadSector(raw.dataPtrPtr, (char*) dataPtrList);
    synchDisk->ReadSector(dataPtrList[index], (char*) dataPtrBuf);
    unsigned sector = dataPtrBuf[virtualSector % NUM_DATAPTR];
    delete [] dataPtrList;
    delete [] dataPtrBuf;
    return sector;
}

/// Return the number of bytes in the file.
unsigned
FileHeader::FileLength() const
{
    return raw.numBytes;
}

/// Print the contents of the file header, and the contents of all the data
/// blocks pointed to by the file header.
void
FileHeader::Print(const char *title)
{
    char *data = new char [SECTOR_SIZE];

    if (title == nullptr) {
        printf("File header:\n");
    } else {
        printf("%s file header:\n", title);
    }

    printf("    size: %u bytes\n"
           "    block indexes: ",
           raw.numBytes);

    unsigned numDirect = min(raw.numSectors, NUM_DIRECT);
    for (unsigned i = 0; i < numDirect; i++) {
        printf("%u ", raw.dataSectors[i]);
    }
    printf("\n");

    for (unsigned i = 0, k = 0; i < numDirect; i++) {
        printf("    contents of block %u:\n", raw.dataSectors[i]);
        synchDisk->ReadSector(raw.dataSectors[i], data);
        for (unsigned j = 0; j < SECTOR_SIZE && k < raw.numBytes; j++, k++) {
            if (isprint(data[j])) {
                printf("%c", data[j]);
            } else {
                printf("\\%X", (unsigned char) data[j]);
            }
        }
        printf("\n");
    }
    delete [] data;

    if (numDirect < raw.numSectors) {
        printf("Contents of indirect blocks omitted.\n");
    }
}

const RawFileHeader *
FileHeader::GetRaw() const
{
    return &raw;
}

bool
FileHeader::IsDirectory() const{
    return raw.directory;
}
