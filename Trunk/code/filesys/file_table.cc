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
}

FileTable::~FileTable()
{
    delete table;
}

SharedFile*
FileTable::Open(unsigned sector)
{
    SharedFile *sharedFile = table->Get(sector);
    if (sharedFile == nullptr) {
        sharedFile = new SharedFile(sector);
        table->SortedInsert(sharedFile, sector);
    }
    sharedFile->fileUsers++;
    return sharedFile;
}

bool
FileTable::MarkForRemove(unsigned sector)
{
    SharedFile *sharedFile = table->Get(sector);
    if (sharedFile == nullptr) {
        return false;
    }
    sharedFile->removeOnDelete = true;
    return true;
}

void
FileTable::Close(SharedFile *sharedFile)
{
    sharedFile->fileUsers--;
    if (sharedFile->fileUsers == 0) {
        table->Remove(sharedFile);
        delete sharedFile;
    }
}
