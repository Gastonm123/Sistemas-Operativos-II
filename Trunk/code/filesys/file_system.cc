/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "directory.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"
#include "threads/system.hh"

#include "file_table.hh"
extern FileTable *fileTable;

#include <stdio.h>
#include <string.h>


/// Sectors containing the file headers for the bitmap of free sectors, and
/// the directory of files.  These file headers are placed in well-known
/// sectors, so that they can be located on boot-up.
static const unsigned FREE_MAP_SECTOR = 0;
static const unsigned DIRECTORY_SECTOR = 1;

/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?
FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
        Directory  *dir     = new Directory(NUM_DIR_ENTRIES);
        FileHeader *mapH    = new FileHeader;
        FileHeader *dirH    = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapH->Allocate(freeMap, FREE_MAP_FILE_SIZE, false));
        ASSERT(dirH->Allocate(freeMap, DIRECTORY_FILE_SIZE, true));

        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapH->WriteBack(FREE_MAP_SECTOR);
        dirH->WriteBack(DIRECTORY_SECTOR);

        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile = new OpenFile(FREE_MAP_SECTOR);
        rootDirFile = new OpenFile(DIRECTORY_SECTOR);

        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        dir->WriteBack(rootDirFile);

        if (debug.IsEnabled('f')) {
            freeMap->Print();
            dir->Print();

            delete freeMap;
            delete dir;
            delete mapH;
            delete dirH;
        }
    } else {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        freeMapFile = new OpenFile(FREE_MAP_SECTOR);
        rootDirFile = new OpenFile(DIRECTORY_SECTOR);
    }

}

FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete rootDirFile;
}

/// Get current working directory. If there is none, the root directory.
OpenFile*
FileSystem::GetCurrentDir()
{
    if (currentThread->currentDirectory)
        return currentThread->currentDirectory;
    return rootDirFile;
}

/// Abre el directorio de la direccion dada.
/// Efecto secundario: el lock del directorio esta tomado.
OpenFile*
FileSystem::OpenDirectory(const char *path) {
    ASSERT(path != nullptr);

    bool success = true;
    char buffer[FILE_NAME_MAX_LEN + 1];
    OpenFile *dirFile;

    if (path[0] == '/') {
        dirFile = rootDirFile;
        path++;
    }
    else {
        dirFile = GetCurrentDir();
    }

    // TODO: revisar cuando tengamos archivos extensibles.
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dirFile->LockFile();

    bool closeDir = false;
    const char* nextBar = strchr(path, '/');
    while (nextBar != nullptr && success) {
        dir->FetchFrom(dirFile);

        unsigned nameLen = nextBar - path;
        if (nameLen > FILE_NAME_MAX_LEN) {
            success = false;
            break;
        }

        memcpy(buffer, path, nameLen);
        buffer[nameLen] = '\0';
 
        int sector = dir->Find(buffer);
        if (sector == -1) {
            // Ruta invalida; el directorio no existe.
            success = false; 
        }
        else {
            OpenFile* temp = new OpenFile(sector);
            if (!temp->IsDirectory()) {
                success = false;
                delete temp;
            }
            else {
                temp->LockFile();
                dirFile->UnlockFile();
                if (closeDir) {
                    delete dirFile;
                }
                else {
                    closeDir = true; 
                }
                dirFile = temp;
                temp = nullptr;
            }
        }
        path = nextBar + 1;
        nextBar = strchr(path, '/');
    }
    if (!success) {
        dirFile->UnlockFile();
        if (closeDir) {
            delete dirFile;
            dirFile = nullptr;
        }
    }
    delete dir;

    return dirFile;
}

/// Traverse the `path` and return the last directory and filename at the end.
///
/// * `path` is the path to a file or directory.
/// * `directory` is a pointer where the last directory's file is stored.
/// * `filename` is a pointer where the filename is stored.
///
/// Additionaly, the lock for `directory` is acquired.
void
FileSystem::FindFile(const char * path, OpenFile **directory, const char **filename)
{
    if (path == nullptr || path[0] == '\0') {
        *directory = nullptr;
        *filename  = nullptr;
    }
    else {
        const char *lastBar = strrchr(path, '/');
        if (lastBar == nullptr) {
            *filename = path;
        }
        else if (lastBar[1] == '\0') {
            *filename = nullptr;
        }
        else {
            *filename = lastBar+1;
        }
        *directory = OpenDirectory(path);
    }
}

/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// `Create` the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.
bool
FileSystem::Create(const char *name, unsigned initialSize)
{
    ASSERT(name != nullptr);
    ASSERT(initialSize < MAX_FILE_SIZE);

    DEBUG('f', "Creating file %s, size %u\n", name, initialSize);

    OpenFile   *dirFile;  //< Directory.
    const char *filename; //< Filename.

    FindFile(name, &dirFile, &filename);

    if (!filename) {
        if (dirFile) {
            dirFile->UnlockFile();
            delete dirFile;
        }
        return false;
    }
    ASSERT(dirFile);

    // TODO: revisar cuando haya archivos ext.
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    bool success = true;
    if (dir->Find(filename) != -1) {
        success = false;  // File is already in directory.
    } else {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        // Lock archivo del bitmap.
        freeMapFile->LockFile();
        freeMap->FetchFrom(freeMapFile);
        int sector = freeMap->Find();
          // Find a sector to hold the file header.
        if (sector == -1) {
            success = false;  // No free block for file header.
        } else if (!dir->Add(filename, sector)) {
            success = false;  // No space in directory.
        } else {
            FileHeader *h = new FileHeader;
            success = h->Allocate(freeMap, initialSize, false);
            // Fails if no space on disk for data.
            if (success) {
                // Everything worked, flush all changes back to disk.
                h->WriteBack(sector);
                freeMap->WriteBack(freeMapFile);
                dir->WriteBack(dirFile);
            }
            delete h;
        }
        freeMapFile->UnlockFile();
        delete freeMap;
    }
    dirFile->UnlockFile();

    if (dirFile != rootDirFile &&
        dirFile != currentThread->currentDirectory) {
        delete dirFile;
    }
    delete dir;

    return success;
}

/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.
OpenFile *
FileSystem::Open(const char *name)
{
    ASSERT(name != nullptr);
    DEBUG('f', "Opening file %s\n", name);

    OpenFile   *dirFile;  //< Directory.
    const char *filename; //< Filename.

    FindFile(name, &dirFile, &filename);

    if (!filename) {
        if (dirFile) {
            dirFile->UnlockFile();
            delete dirFile;
        }
        return nullptr;
    }
    ASSERT(dirFile);

    OpenFile *openFile = nullptr;
    Directory *dir     = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    int sector = dir->Find(filename);
    if (sector >= 0) {
        openFile = new OpenFile(sector);  // `filename` was found in directory.
        if (openFile->IsDirectory()) {
            // El archivo es un directorio; operacion invalida.
            delete openFile;
            openFile = nullptr;
        }
    }
    dirFile->UnlockFile();

    if (dirFile != rootDirFile &&
        dirFile != currentThread->currentDirectory) {
        delete dirFile;
    }
    delete dir;

    return openFile;  // Return null if not found o es un directorio.
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool
FileSystem::Remove(const char *name)
{
    ASSERT(name != nullptr);
    DEBUG('f', "Removing file %s\n", name);

    OpenFile   *dirFile;  //< Directory.
    const char *filename; //< Filename.

    FindFile(name, &dirFile, &filename);

    if (!filename) {
        if (dirFile) {
            dirFile->UnlockFile();
            delete dirFile;
        }
        return false;
    }
    ASSERT(dirFile);

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    int sector = dir->Find(filename);
    bool success = false;
    if (sector >= 0) {
        DEBUG('f', "Removing file header from sector %d\n", sector);

        FileHeader *fileH = new FileHeader;
        fileH->FetchFrom(sector);

        if (!fileH->IsDirectory()) {
            /// If the file is being used the remove will be later.
            if (fileTable->MarkForRemove(sector)) {
                DEBUG('f', "File is being used, removing later.\n");
                dir->Remove(filename);
                dir->WriteBack(dirFile);    // Flush to disk.
                dirFile->UnlockFile();
            }
            else {
                Bitmap *freeMap = new Bitmap(NUM_SECTORS);
                // Lock archivo de bitmap.
                freeMapFile->LockFile();
                freeMap->FetchFrom(freeMapFile);

                fileH->Deallocate(freeMap);  // Remove data blocks.
                freeMap->Clear(sector);      // Remove header block.
                dir->Remove(filename);

                dir->WriteBack(dirFile);    // Flush to disk.
                dirFile->UnlockFile();
                freeMap->WriteBack(freeMapFile);  // Flush to disk.
                freeMapFile->UnlockFile();
                delete freeMap;
            }
            success = true;
        }

        delete fileH;
    }

    if (!success) {
        dirFile->UnlockFile();
    }

    if (dirFile != rootDirFile &&
        dirFile != currentThread->currentDirectory) {
        delete dirFile;
    }
    delete dir;

    return success;
}

bool
FileSystem::MakeDirectory(const char *name)
{
    ASSERT(name != nullptr);
    DEBUG('f', "Creating dir %s", name);

    OpenFile   *dirFile;  //< Directory.
    const char *filename; //< Filename.

    /// NOTICE: si el nombre del directorio incluye una / al final, la funcion
    /// FindFile va a fallar.
    FindFile(name, &dirFile, &filename);

    if (!filename) {
        if (dirFile) {
            dirFile->UnlockFile();
            delete dirFile;
        }
        return false;
    }
    ASSERT(dirFile);

    // TODO: revisar cuando haya archivos ext.
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    bool success = true;
    if (dir->Find(filename) != -1) {
        success = false;  // File is already in directory.
    } else {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        // Lock archivo del bitmap.
        freeMapFile->LockFile();
        freeMap->FetchFrom(freeMapFile);
        int sector = freeMap->Find();
          // Find a sector to hold the file header.
        if (sector == -1) {
            success = false;  // No free block for file header.
        } else if (!dir->Add(filename, sector)) {
            success = false;  // No space in directory.
        } else {
            FileHeader *h = new FileHeader;
            unsigned size = sizeof (DirectoryEntry) * NUM_DIR_ENTRIES;
            success = h->Allocate(freeMap, size, true);
            // Fails if no space on disk for data.
            if (success) {
                // Everything worked, flush all changes back to disk.
                h->WriteBack(sector);
                freeMap->WriteBack(freeMapFile);
                dir->WriteBack(dirFile);
            }
            delete h;
        }
        freeMapFile->UnlockFile();
        delete freeMap;
    }
    dirFile->UnlockFile();

    if (dirFile != rootDirFile &&
        dirFile != currentThread->currentDirectory) {
        delete dirFile;
    }
    delete dir;

    return success;
}

bool
FileSystem::ChangeDirectory(const char* name) {
    ASSERT(name != nullptr);
    
    OpenFile   *dirFile;  //< Directory.
    const char *filename; //< Filename.

    FindFile(name, &dirFile, &filename);

    /// Si el nombre del directorio incluye una / al final, dirFile es nuestro
    /// target.
    if (!filename) {
        if (dirFile) {
            if (currentThread->currentDirectory != nullptr) {
                delete currentThread->currentDirectory;
            }
            currentThread->currentDirectory = dirFile;
            dirFile->UnlockFile();
            return true;
        }
        return false;
    }
    ASSERT(dirFile);

    bool success = true;
    OpenFile  *openDir = nullptr;
    Directory *dir     = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    int sector = dir->Find(filename);
    if (sector >= 0) {
        openDir = new OpenFile(sector);  // `filename` was found in directory.
        if (!openDir->IsDirectory()) {
            // El archivo no es un directorio; operacion invalida.
            delete openDir;
            success = false;
        }
    }
    dirFile->UnlockFile();

    if (dirFile != rootDirFile &&
        dirFile != currentThread->currentDirectory) {
        delete dirFile;
    }
    delete dir;

    if (success) {
        if (currentThread->currentDirectory != nullptr) {
            delete currentThread->currentDirectory;
        }
        currentThread->currentDirectory = openDir;
    }
         
    return success;
}

bool
FileSystem::ListDirectory(const char* name) {
    ASSERT(name != nullptr);

    OpenFile   *dirFile;  //< Directory.
    const char *filename; //< Filename.

    FindFile(name, &dirFile, &filename);

    /// Si el nombre del directorio incluye una / al final, dirFile es nuestro
    /// target.
    if (!filename) {
        if (dirFile) {
            Directory *dir = new Directory(NUM_DIR_ENTRIES);
            dir->FetchFrom(dirFile);
            dirFile->UnlockFile();
            dir->List();
            delete dir;
            delete dirFile;
            return true;
        }
        return false;
    }
    ASSERT(dirFile);

    bool success = true;
    OpenFile *file;
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    int sector = dir->Find(filename);
    if (sector >= 0) {
        file = new OpenFile(sector);  // `filename` was found in directory.
        if (!file->IsDirectory()) {
            // El archivo no es un directorio; operacion invalida.
            success = false;
        }
        else {
            dir->FetchFrom(file);
        }
        delete file;
    }
    dirFile->UnlockFile();

    if (dirFile != rootDirFile &&
        dirFile != currentThread->currentDirectory) {
        delete dirFile;
    }

    if (success) {
        dir->List(); 
    }

    delete dir;
    return success;
}

bool
FileSystem::RemoveDirectory(const char *name)
{
    ASSERT(name != nullptr);
    DEBUG('f', "Removing file %s\n", name);

    OpenFile   *dirFile;  //< Directory.
    const char *filename; //< Filename.

    FindFile(name, &dirFile, &filename);

    /// TODO: Si el nombre del directorio incluye una / al final, dirFile es
    /// nuestro target.
    if (!filename) {
        if (dirFile) {
            dirFile->UnlockFile();
            delete dirFile;
        }
        return false;
    }
    ASSERT(dirFile);

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    int sector = dir->Find(filename);
    if (sector == -1) {
        dirFile->UnlockFile();
        if (dirFile != rootDirFile &&
            dirFile != currentThread->currentDirectory) {
            delete dirFile;
        }
        return false;  // file not found
    }

    DEBUG('f', "Removing file header from sector %d\n", sector);

    FileHeader *fileH = new FileHeader;
    fileH->FetchFrom(sector);

    if (!fileH->IsDirectory()) {
        dirFile->UnlockFile();
        if (dirFile != rootDirFile &&
            dirFile != currentThread->currentDirectory) {
            delete dirFile;
        }
        delete dir;
        return false;   // no es un directorio
    }

    if (fileTable->Used(sector)) {
        dirFile->UnlockFile();
        if (dirFile != rootDirFile &&
            dirFile != currentThread->currentDirectory) {
            delete dirFile;
        }
        delete dir;
        return false;   // el directorio esta abierto.
    }

    OpenFile *subdirFile = new OpenFile(sector);
    Directory *subdir    = new Directory(NUM_DIR_ENTRIES);
    subdirFile->LockFile();
    subdir->FetchFrom(subdirFile);
    bool empty = subdir->Empty();
    subdirFile->UnlockFile();
    delete subdirFile;
    delete subdir;    

    if (!empty) {
        dirFile->UnlockFile();
        if (dirFile != rootDirFile &&
            dirFile != currentThread->currentDirectory) {
            delete dirFile;
        }
        delete dir;
        return false;   // el directorio no esta vacio.
    }

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    // Lock archivo de bitmap.
    freeMapFile->LockFile();
    freeMap->FetchFrom(freeMapFile);

    fileH->Deallocate(freeMap);  // Remove data blocks.
    freeMap->Clear(sector);      // Remove header block.
    dir->Remove(filename);

    dir->WriteBack(dirFile);    // Flush to disk.
    dirFile->UnlockFile();
    freeMap->WriteBack(freeMapFile);  // Flush to disk.
    freeMapFile->UnlockFile();

    if (dirFile != rootDirFile &&
        dirFile != currentThread->currentDirectory) {
        delete dirFile;
    }
    delete fileH;
    delete dir;
    delete freeMap;
    return true;
}

/// Liberate a file's blocks after it is no longer used.
void FileSystem::Liberate(unsigned sector)
{
    /// The file is unused and unreacheable. No need to acquire lock.
    FileHeader *fileH = new FileHeader;
    fileH->FetchFrom(sector);

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);

    fileH->Deallocate(freeMap);  // Remove data blocks.
    freeMap->Clear(sector);      // Remove header block.

    freeMap->WriteBack(freeMapFile);
    delete freeMap;
    delete fileH;
}

/// List all the files in the file system directory.
/// * DEPRECATED *
void
FileSystem::List()
{
    Directory *dir = new Directory(NUM_DIR_ENTRIES);

    dir->FetchFrom(rootDirFile);
    dir->List();
    delete dir;
}

static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector)) {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value) {
        DEBUG('f', "Error: %s\n", message);
    }
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    if (CheckForError(sector < NUM_SECTORS,
                      "sector number too big.  Skipping bitmap check.")) {
        return true;
    }
    return CheckForError(AddToShadowBitmap(sector, shadowMap),
                         "sector number already used.");
}

/// It is necessary to use the disk defined in system.cc to access indirect
/// blocks of file headers.
#include "synch_disk.hh"
extern SynchDisk *synchDisk;

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
                                                        SECTOR_SIZE),
                           "sector count not compatible with file size.");
    error |= CheckForError(rh->numSectors < NUM_DIRECT,
                           "too many blocks.");
    unsigned remaining = rh->numSectors;
    for (unsigned i = 0; remaining > 0 && i < NUM_DIRECT; i++) {
        unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap);
        remaining--;
    }

    if (!error && remaining) {
        unsigned *dataPtrBuf = new unsigned[NUM_DATAPTR];
        synchDisk->ReadSector(rh->dataPtr, (char*) dataPtrBuf);
        for (unsigned i = 0; remaining > 0 && i < NUM_DATAPTR; i++) {
            unsigned s = dataPtrBuf[i];
            error |= CheckSector(s, shadowMap);
            remaining--;
        }
        delete [] dataPtrBuf;
    }

    if (!error && remaining) {
        unsigned *dataPtrList = new unsigned[NUM_DATAPTR];
        unsigned *dataPtrBuf  = new unsigned[NUM_DATAPTR];
        unsigned c = 0; //< position inside dataPtrBuf.
        unsigned sector = 0; //< which sector of dataPtrList.
        synchDisk->ReadSector(rh->dataPtrPtr, (char*) dataPtrList);
        synchDisk->ReadSector(dataPtrList[sector], (char*) dataPtrBuf);
        for (; remaining > 0; remaining--) {
            unsigned s = dataPtrBuf[c];
            error |= CheckSector(s, shadowMap);

            if (++c == NUM_DATAPTR) {
                synchDisk->ReadSector(dataPtrList[++sector], (char*)
                                      dataPtrBuf);
                c = 0;
            }
        }
        delete [] dataPtrBuf;
        delete [] dataPtrList;
    }

    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "inconsistent bitmap.");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    const char *knownNames[NUM_DIR_ENTRIES];

    for (unsigned i = 0; i < NUM_DIR_ENTRIES; i++) {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse) {
            if (strlen(e->name) > FILE_NAME_MAX_LEN) {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++) {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0) {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated) {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);

            if (rh->directory) {
                Directory *dir = new Directory(NUM_DIR_ENTRIES);
                OpenFile *dirFile = new OpenFile(e->sector);
                dirFile->LockFile();
                dir->FetchFrom(dirFile);
                error |= CheckDirectory(dir->GetRaw(), shadowMap);
                dirFile->UnlockFile();
                delete dir;
                delete dirFile;
            }

            delete h;
        }
    }
    return error;
}

bool
FileSystem::Check()
{
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    freeMapFile->LockFile();
    rootDirFile->LockFile();

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader;
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "bad bitmap header: wrong file size.");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "bad bitmap header: wrong number of sectors.");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader;
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(rootDirFile);
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    DEBUG('f', error ? "Filesystem check failed.\n"
                     : "Filesystem check succeeded.\n");

    freeMapFile->UnlockFile();
    rootDirFile->UnlockFile();

    return !error;
}

/// Asume exclusion mutua.
void
FileSystem::PrintDirectory(Directory *dir, bool recursive) {
    dir->Print();
    if (!recursive) {
        return;
    }

    const RawDirectory *rd = dir->GetRaw(); 

    for (unsigned i = 0; i < NUM_DIR_ENTRIES; i++) {
        const DirectoryEntry *e = &rd->table[i];
        if (e->inUse) {
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            if (rh->directory) {
                Directory *subdir = new Directory(NUM_DIR_ENTRIES);
                OpenFile *subdirFile = new OpenFile(e->sector);
                printf("--------------------------------\n");
                subdirFile->LockFile();
                subdir->FetchFrom(subdirFile);
                PrintDirectory(subdir, true); 
                subdirFile->UnlockFile();
                printf("--------------------------------\n");
                delete subdir;
                delete subdirFile;
            }
            delete h;
        }
    }
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void
FileSystem::Print(bool recursive)
{
    FileHeader *bitH    = new FileHeader;
    FileHeader *dirH    = new FileHeader;
    Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
    Directory  *dir     = new Directory(NUM_DIR_ENTRIES);

    printf("--------------------------------\n");
    bitH->FetchFrom(FREE_MAP_SECTOR);
    bitH->Print("Bitmap");

    printf("--------------------------------\n");
    dirH->FetchFrom(DIRECTORY_SECTOR);
    dirH->Print("Directory");

    printf("--------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("--------------------------------\n");
    rootDirFile->LockFile();
    dir->FetchFrom(rootDirFile);
    PrintDirectory(dir, recursive);
    rootDirFile->UnlockFile();
    printf("--------------------------------\n");

    delete bitH;
    delete dirH;
    delete freeMap;
    delete dir;
}
