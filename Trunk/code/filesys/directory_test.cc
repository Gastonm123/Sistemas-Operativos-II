#include "threads/system.hh"
#include <stdio.h>
#include <string.h>

const unsigned FILE_LEN = 15;

/// Proceso que crea muchos archivos en el directorio raiz.
void
Spam(void *dummy)
{
    unsigned *offset = (unsigned *)dummy;
    for (int i = 0; i < 10; i++) {
        char name[10];
        sprintf(name, "spam%d", (*offset)*10+i);
        fileSystem->Create(name, 10);
    }
    for (int i = 0; i < 10; i++) {
        char name[10];
        sprintf(name, "spam%d", (*offset)*10+i);
        fileSystem->Remove(name);
    }
}

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
    ASSERT(fileSystem->Remove("/new_dir/sub_dir/new_file"));
    ASSERT(fileSystem->ChangeDirectory("/"));
    ASSERT(fileSystem->RemoveDirectory("/new_dir/sub_dir"));
    ASSERT(fileSystem->RemoveDirectory("/new_dir"));

    printf("Test de contencion iniciando.\n");
    /// Test de contencion de locks para el directorio raiz.
    Thread *spam1 = new Thread("spam1", true);
    Thread *spam2 = new Thread("spam2", true);

    unsigned offset1 = 0;
    unsigned offset2 = 0;
    spam1->Fork(Spam, &offset1);
    spam2->Fork(Spam, &offset2);

    spam1->Join();
    spam2->Join();
    printf("Test de contencion exitoso.\n");

    currentThread->Finish();
}
