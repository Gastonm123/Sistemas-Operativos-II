#ifndef NACHOS_FILESYS_SHAREDFILE_HH
#define NACHOS_FILESYS_SHAREDFILE_HH

class Lock;
class FileHeader;

#include "lib/list.hh"

class SharedFile {
public:
    SharedFile(unsigned sector);

    ~SharedFile();

    /// I-nodo del archivo.
    FileHeader *fileHeader;

    /// Lock para lectura/escritura.
    Lock *fileLock;

    /// Bandera para senialar que el archivo debe ser borrado del disco cuando
    /// se deje de usar.
    bool removeOnDelete;

    /// Conteo de usuarios para un archivo.
    unsigned fileUsers;

    /// Header block.
    unsigned sector;
};

class FileTable {
public:
    FileTable();

    ~FileTable();

    /// Fetch an existing shared file or append a new one to the table and 
    /// return the reference.
    SharedFile *Open(unsigned sector);

    /// Mark a file to be removed after it is closed by all its users.
    /// Return true if the file was marked, false if the file was not in the
    /// table.
    bool MarkForRemove(unsigned sector);

    /// Decrease the user count and delete the reference when no one is using
    /// it.
    void Close(SharedFile *sharedFile);
private:
    List<SharedFile*> *table;
};

#endif
