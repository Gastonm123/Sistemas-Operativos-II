#include "file_header.hh"
#include "threads/system.hh"

SharedFile::SharedFile(unsigned _sector)
{
    fileHeader = new FileHeader;
    fileHeader->FetchFrom(_sector);
    fileLock  = new Lock("file lock");
    fileUsers = 0;
    sector    = _sector;
    removeOnDelete = false;
}

SharedFile::~SharedFile()
{
    ASSERT(fileUsers == 0);
    if (removeOnDelete) {
        fileSystem->Liberate(sector);
    }
    delete fileHeader;
    delete fileLock;
}

FileTable::FileTable()
{
    table = new List<SharedFile*>;
    lock  = new Lock("file table lock");
}

FileTable::~FileTable()
{
    delete table;
    delete lock;
}

const SharedFile*
FileTable::Open(unsigned sector)
{
    lock->Acquire();
    SharedFile *sharedFile = table->Get(sector);
    if (sharedFile == nullptr) {
        sharedFile = new SharedFile(sector);
        table->SortedInsert(sharedFile, sector);
    }
    sharedFile->fileUsers++;
    lock->Release();
    return sharedFile;
}

bool
FileTable::MarkForRemove(unsigned sector)
{
    lock->Acquire();
    SharedFile *sharedFile = table->Get(sector);
    if (sharedFile == nullptr) {
        lock->Release();
        return false;
    }
    sharedFile->removeOnDelete = true;
    lock->Release();
    return true;
}

void
FileTable::Close(unsigned sector)
{
    lock->Acquire();
    SharedFile *sharedFile = table->Get(sector);
    if (sharedFile) {
        sharedFile->fileUsers--;
        if (sharedFile->fileUsers == 0) {
            table->Remove(sharedFile);
            delete sharedFile;
        }
    }
    lock->Release();
}

bool
FileTable::Used(unsigned sector)
{
    lock->Acquire();
    bool used = table->Get(sector) != nullptr;
    lock->Release();
    return used;
}

void
SharedFilePrint(SharedFile *sharedFile)
{
    printf("Open file:\n");
    printf("    sector: %d, users: %d, lock: %d\n"
           "    markForRemove: %d\n", sharedFile->sector, sharedFile->fileUsers,
           sharedFile->fileLock->IsHeldByCurrentThread(),
           sharedFile->removeOnDelete);
}

void
FileTable::Print()
{
    lock->Acquire();
    printf("File table contents:\n");
    table->Apply(SharedFilePrint);
    lock->Release();
}
