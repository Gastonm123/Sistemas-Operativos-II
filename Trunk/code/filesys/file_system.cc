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

        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);

        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        dir->WriteBack(directoryFile);

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
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);
    }

}

FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
}

/// Abre el directorio de la direccion dada.
/// Corresponde al caller chequear que el directorio no sea raiz/current.
/// Efecto secundario: el lock del directorio esta tomado.
OpenFile*
FileSystem::OpenDirectory(const char *path) {
    ASSERT(path != nullptr);

    bool success = true;
    char buffer[FILE_NAME_MAX_LEN + 1];
    OpenFile *dirFile;

    bool rootDir = false;
    if (*path == '/') {
        rootDir = true;
        path++;
    }
    else if (currentThread->currentDirectory == nullptr) {
        rootDir = true;
    }

    if (*path == '\0') {
        return nullptr;
    }

    if (rootDir) {
        dirFile = directoryFile;
    }
    else {
        dirFile = currentThread->currentDirectory;
    }

    // TODO: revisar cuando tengamos archivos extensibles.
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dirFile->LockFile();

    bool closeDir = false;
    const char* nextBar = strchr(path, '/');
    while (nextBar != nullptr && success) {
        dir->FetchFrom(dirFile);

        strncpy(buffer, path, nextBar - path);
        buffer[nextBar - path] = '\0';
 
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
        if (*path == '\0') {
            success = false;
        }
        else {
            nextBar = strchr(path, '/');
        }
    }
    if (!success) {
        dirFile->UnlockFile();
        if (closeDir) {
            delete dirFile;
            dirFile = nullptr;
        }
    }

    return dirFile;
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

    OpenFile *dirFile;
    bool closeDir;

    if (*name == '/' && strchr(name + 1, '/') == nullptr) {
        name = name + 1;
        if (*name == '\0') {
            return false;
        }
        dirFile = directoryFile;
        closeDir = false;
        dirFile->LockFile();
    }
    else if (strchr(name, '/') == nullptr) {
        if (*name == '\0') {
            return false;
        }
        if (currentThread->currentDirectory == nullptr) {
            dirFile = directoryFile;
        }
        else {
            dirFile = currentThread->currentDirectory;
        }
        closeDir = false;
        dirFile->LockFile();
    }
    else {
        dirFile = OpenDirectory(name);
        if (dirFile == nullptr) {
            return false;
        }
        closeDir = true;
        name = strrchr(name, '/') + 1;
    }

    // TODO: revisar cuando haya archivos ext.
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    bool success = true;
    if (dir->Find(name) != -1) {
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
        } else if (!dir->Add(name, sector)) {
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
    if (closeDir) {
        delete dirFile;
    }

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

    OpenFile *dirFile;
    bool closeDir;

    if (*name == '/' && strchr(name + 1, '/') == nullptr) {
        name = name + 1;
        if (*name == '\0') {
            return nullptr;
        }
        dirFile = directoryFile;
        closeDir = false;
        dirFile->LockFile();
    }
    else if (strchr(name, '/') == nullptr) {
        if (*name == '\0') {
            return nullptr;
        }
        if (currentThread->currentDirectory == nullptr) {
            dirFile = directoryFile;
        }
        else {
            dirFile = currentThread->currentDirectory;
        }
        closeDir = false;
        dirFile->LockFile();
    }
    else {
        dirFile = OpenDirectory(name);
        if (dirFile == nullptr) {
            return nullptr;
        }
        closeDir = true;
        name = strrchr(name, '/') + 1;
    }

    OpenFile  *openFile = nullptr;
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    int sector = dir->Find(name);
    if (sector >= 0) {
        openFile = new OpenFile(sector);  // `name` was found in directory.
        if (openFile->IsDirectory()) {
            // El archivo es un directorio; operacion invalida.
            delete openFile;
            openFile = nullptr;
        }
    }
    dirFile->UnlockFile();
    if (closeDir) {
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

    OpenFile *dirFile;
    bool closeDir;

    if (*name == '/' && strchr(name + 1, '/') == nullptr) {
        name = name + 1;
        if (*name == '\0') {
            return false;
        }
        dirFile = directoryFile;
        closeDir = false;
        dirFile->LockFile();
    }
    else if (strchr(name, '/') == nullptr) {
        if (*name == '\0') {
            return false;
        }
        if (currentThread->currentDirectory == nullptr) {
            dirFile = directoryFile;
        }
        else {
            dirFile = currentThread->currentDirectory;
        }
        closeDir = false;
        dirFile->LockFile();
    }
    else {
        dirFile = OpenDirectory(name);
        if (dirFile == nullptr) {
            return false;
        }
        closeDir = true;
        name = strrchr(name, '/') + 1;
    }


    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    int sector = dir->Find(name);
    if (sector == -1) {
        dirFile->UnlockFile();
        if (closeDir) {
            delete dir;
        }
        return false;  // file not found
    }

    DEBUG('f', "Removing file header from sector %d\n", sector);

    FileHeader *fileH = new FileHeader;
    fileH->FetchFrom(sector);

    if (fileH->IsDirectory()) {
        dirFile->UnlockFile();
        if (closeDir) {
            delete dir;
        }
        return false;   // es un directorio
    }

    /// If the file is being used the remove will be later.
    if (fileTable->MarkForRemove(sector)) {

        DEBUG('f', "File is being used, removing later.\n");

        dir->Remove(name);
        dir->WriteBack(dirFile);    // Flush to disk.
        dirFile->UnlockFile();
        if (closeDir) {
            delete dir;
        }
        delete fileH;
        return true;
    }

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    // Lock archivo de bitmap.
    freeMapFile->LockFile();
    freeMap->FetchFrom(freeMapFile);

    fileH->Deallocate(freeMap);  // Remove data blocks.
    freeMap->Clear(sector);      // Remove header block.
    dir->Remove(name);

    dir->WriteBack(dirFile);    // Flush to disk.
    dirFile->UnlockFile();
    freeMap->WriteBack(freeMapFile);  // Flush to disk.
    freeMapFile->UnlockFile();

    delete fileH;
    if (closeDir) {
        delete dir;
    }
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
void
FileSystem::List()
{
    Directory *dir = new Directory(NUM_DIR_ENTRIES);

    dir->FetchFrom(directoryFile);
    dir->List();
    delete dir;
}

bool
FileSystem::MakeDirectory(const char *name)
{
    ASSERT(name != nullptr);

    int size = sizeof (DirectoryEntry) * NUM_DIR_ENTRIES;   

    DEBUG('f', "Creating dir %s", name);

    OpenFile *dirFile;
    bool closeDir;

    if (*name == '/' && strchr(name + 1, '/') == nullptr) {
        name = name + 1;
        if (*name == '\0') {
            return false;
        }
        dirFile = directoryFile;
        closeDir = false;
        dirFile->LockFile();
    }
    else if (strchr(name, '/') == nullptr) {
        if (*name == '\0') {
            return false;
        }
        if (currentThread->currentDirectory == nullptr) {
            dirFile = directoryFile;
        }
        else {
            dirFile = currentThread->currentDirectory;
        }
        closeDir = false;
        dirFile->LockFile();
    }
    else {
        dirFile = OpenDirectory(name);
        if (dirFile == nullptr) {
            return false;
        }
        closeDir = true;
        name = strrchr(name, '/') + 1;
    }

    // TODO: revisar cuando haya archivos ext.
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    bool success = true;
    if (dir->Find(name) != -1) {
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
        } else if (!dir->Add(name, sector)) {
            success = false;  // No space in directory.
        } else {
            FileHeader *h = new FileHeader;
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
    if (closeDir) {
        delete dirFile;
    }

    return success;
}

bool
FileSystem::ChangeDirectory(const char* name) {
    ASSERT(name != nullptr);
    
    OpenFile *dirFile;
    bool closeDir;

    if (*name == '/' && strchr(name + 1, '/') == nullptr) {
        name = name + 1;
        if (*name == '\0') {
            if (currentThread->currentDirectory != nullptr) {
                delete currentThread->currentDirectory;
            }
            currentThread->currentDirectory = new OpenFile(DIRECTORY_SECTOR);
            return true;
        } 
        dirFile = directoryFile;
        closeDir = false;
        dirFile->LockFile();
    }
    else if (strchr(name, '/') == nullptr) {
        if (*name == '\0') {
            return false;
        }
        if (currentThread->currentDirectory == nullptr) {
            dirFile = directoryFile;
        }
        else {
            dirFile = currentThread->currentDirectory;
        }
        closeDir = false;
        dirFile->LockFile();
    }
    else {
        dirFile = OpenDirectory(name);
        if (dirFile == nullptr) {
            return false;
        }
        closeDir = true;
        name = strrchr(name, '/') + 1;
    }

    bool success = true;

    OpenFile  *openDir = nullptr;
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    int sector = dir->Find(name);
    if (sector >= 0) {
        openDir = new OpenFile(sector);  // `name` was found in directory.
        if (!openDir->IsDirectory()) {
            // El archivo no es un directorio; operacion invalida.
            delete openDir;
            success = false;
        }
    }
    dirFile->UnlockFile();
    if (closeDir) {
        delete dirFile;
    }

    if (success) {
        if (currentThread->currentDirectory != nullptr) {
            delete currentThread->currentDirectory;
        }
        currentThread->currentDirectory = openDir;
    }
         
    delete dir;
    return success;
}

bool
FileSystem::ListDirectory(const char* name) {
    ASSERT(name != nullptr);

    Directory* dir = new Directory(NUM_DIR_ENTRIES);    
    OpenFile *dirFile;
    bool closeDir;

    if (*name == '/' && strchr(name + 1, '/') == nullptr) {
        name = name + 1;
        if (*name == '\0') {
            dir->FetchFrom(directoryFile);
            dir->List();
            delete dir;
            return true;
        } 
        dirFile = directoryFile;
        closeDir = false;
        dirFile->LockFile();
    }
    else if (strchr(name, '/') == nullptr) {
        if (*name == '\0') {
            delete dir;
            return false;
        }
        if (currentThread->currentDirectory == nullptr) {
            dirFile = directoryFile;
        }
        else {
            dirFile = currentThread->currentDirectory;
        }
        closeDir = false;
        dirFile->LockFile();
    }
    else {
        dirFile = OpenDirectory(name);
        if (dirFile == nullptr) {
            delete dir;
            return false;
        }
        closeDir = true;
        name = strrchr(name, '/') + 1;
    }

    bool success = true;

    OpenFile *temp;
    dir->FetchFrom(dirFile);
    int sector = dir->Find(name);
    if (sector >= 0) {
        temp = new OpenFile(sector);  // `name` was found in directory.
        if (!temp->IsDirectory()) {
            // El archivo no es un directorio; operacion invalida.
            success = false;
        }
        else {
            dir->FetchFrom(temp);
        }
        delete temp;
    }

    dirFile->UnlockFile();
    if (closeDir) {
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

    OpenFile *dirFile;
    bool closeDir;

    if (*name == '/' && strchr(name + 1, '/') == nullptr) {
        name = name + 1;
        if (*name == '\0') {
            return false;
        }
        dirFile = directoryFile;
        closeDir = false;
        dirFile->LockFile();
    }
    else if (strchr(name, '/') == nullptr) {
        if (*name == '\0') {
            return false;
        }
        if (currentThread->currentDirectory == nullptr) {
            dirFile = directoryFile;
        }
        else {
            dirFile = currentThread->currentDirectory;
        }
        closeDir = false;
        dirFile->LockFile();
    }
    else {
        dirFile = OpenDirectory(name);
        if (dirFile == nullptr) {
            return false;
        }
        closeDir = true;
        name = strrchr(name, '/') + 1;
    }


    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(dirFile);
    int sector = dir->Find(name);
    if (sector == -1) {
        dirFile->UnlockFile();
        if (closeDir) {
            delete dir;
        }
        return false;  // file not found
    }

    DEBUG('f', "Removing file header from sector %d\n", sector);

    FileHeader *fileH = new FileHeader;
    fileH->FetchFrom(sector);

    if (!fileH->IsDirectory()) {
        dirFile->UnlockFile();
        if (closeDir) {
            delete dir;
        }
        return false;   // no es un directorio
    }

    if (fileTable->Used(sector)) {
        dirFile->UnlockFile();
        if (closeDir) {
            delete dir;
        }
        return false;   // el directorio esta abierto.
    }

    OpenFile *subdirFile = new OpenFile(sector);
    Directory *subdir = new Directory(NUM_DIR_ENTRIES);
    subdirFile->LockFile();
    subdir->FetchFrom(subdirFile);
    bool empty = subdir->Empty();
    subdirFile->UnlockFile();
    delete subdirFile;
    delete subdir;    

    if (!empty) {
        dirFile->UnlockFile();
        if (closeDir) {
            delete dir;
        }
        return false;   // el directorio no esta vacio.
    }

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    // Lock archivo de bitmap.
    freeMapFile->LockFile();
    freeMap->FetchFrom(freeMapFile);

    fileH->Deallocate(freeMap);  // Remove data blocks.
    freeMap->Clear(sector);      // Remove header block.
    dir->Remove(name);

    dir->WriteBack(dirFile);    // Flush to disk.
    dirFile->UnlockFile();
    freeMap->WriteBack(freeMapFile);  // Flush to disk.
    freeMapFile->UnlockFile();

    delete fileH;
    if (closeDir) {
        delete dir;
    }
    delete freeMap;
    return true;
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
    directoryFile->LockFile();

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
    dir->FetchFrom(directoryFile);
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
    directoryFile->UnlockFile();

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
    directoryFile->LockFile();
    dir->FetchFrom(directoryFile);
    PrintDirectory(dir, recursive);
    directoryFile->UnlockFile();
    printf("--------------------------------\n");

    delete bitH;
    delete dirH;
    delete freeMap;
    delete dir;
}
