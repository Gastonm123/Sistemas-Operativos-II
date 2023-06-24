#include "threads/system.hh"
#include <stdio.h>
#include <string.h>

const unsigned FILE_LEN = 15;

/// Script para testear espacio jerarquico del sistema de archivos.
/// TODO: testear concurrencia en directorios.
/// TODO: tratar de complejizar el test.
void DirectoryTest()
{
    ASSERT(fileSystem->MakeDirectory("new_dir"));

    ASSERT(fileSystem->Create("new_dir/new_file", 15));
    ASSERT(fileSystem->ListDirectory("new_dir"));

    OpenFile *file = fileSystem->Open("new_dir/new_file");
    ASSERT(file);
    
    file->Write("archivo nuevoo", 15);
    char buffer[20];
    file->Seek(0);
    file->Read(buffer, 15);
    printf("Read %s\n", buffer);
    
    ASSERT(fileSystem->ChangeDirectory("new_dir"));

    ASSERT(fileSystem->MakeDirectory("sub_dir"));

    ASSERT(fileSystem->Create("sub_dir/new_file", 100));
    OpenFile *file2 = fileSystem->Open("/new_dir/sub_dir/new_file");
    ASSERT(file2);
    file2->Write("123456789123456789", 18);
    delete file2;

    ASSERT(fileSystem->Remove("new_file"));
    ASSERT(fileSystem->ChangeDirectory("/new_dir/sub_dir"));
    ASSERT(fileSystem->ListDirectory("/new_dir"));

    delete file;
    currentThread->Finish();
}
