#include "threads/system.hh"
#include <stdio.h>
#include <string.h>

const unsigned FILE_LEN = 15;

/// Script para probar la capacidad de concurrencia del sistema de archivos.
/// Correr nachos con las opciones -ct y -rs 10 varias veces para ver un poco de
/// interleaving.
void
Produce(void *dummy)
{
    DEBUG('f', "Producer start.\n");
    OpenFile *file = fileSystem->Open("prod_cons");
    ASSERT(file);
    for (unsigned i = 0; i < 10; i++) {
        char num = '0' + i;
        file->Write(&num, 1);
        printf("Wrote %d\n", i);
        currentThread->Yield();
    }
}

void
Consume(void *dummy)
{
    DEBUG('f', "Consumer start.\n");
    OpenFile *file = fileSystem->Open("prod_cons");
    ASSERT(file);
    char contents[FILE_LEN] = "";
    while (strlen(contents) < 10) {
        file->Read(contents, FILE_LEN);
        file->Seek(0); // Importante!
        printf("Read %s\n", contents);
        currentThread->Yield();
    }
}

void ConcurrentFileSysTest()
{
    fileSystem->Create("prod_cons", FILE_LEN);

    Thread *producer = new Thread("Producer", true);
    Thread *consumer = new Thread("Consumer", true);

    producer->Fork(Produce, nullptr);
    consumer->Fork(Consume, nullptr);

    producer->Join();
    DEBUG('f', "Producer finished.\n");
    consumer->Join();
    DEBUG('f', "Consumer finished.\n");
    fileSystem->Remove("prod_cons");
    currentThread->Finish();
}
