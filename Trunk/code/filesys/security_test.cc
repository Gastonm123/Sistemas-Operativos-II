#include "threads/system.hh"
#include <string.h>
#include <stdio.h>

void SecurityFileSysTest()
{
    // Test de seguridad.
    fileSystem->Create("prod_cons", SECTOR_SIZE);
    OpenFile *file = fileSystem->Open("prod_cons");
    char contents[SECTOR_SIZE];
    file->Read(contents, SECTOR_SIZE);
    char expected[SECTOR_SIZE] = {}; // todos ceros.
    ASSERT(memcmp(contents, expected, SECTOR_SIZE) == 0);

    // Escribe un cero para extender el archivo.
    char zero = 0;
    file->Write(&zero, 1);
    file->Seek(SECTOR_SIZE);
    file->Read(contents, SECTOR_SIZE);
    ASSERT(memcmp(contents, expected, SECTOR_SIZE) == 0);

    // Clean-up.
    delete file;
    fileSystem->Remove("prod_cons");
    currentThread->Finish();
}
