#include "threads/system.hh"
#include <cstdio>
#include <cstring>

/// Script para testear archivos extensibles
void ExtensibleFileTest()
{

	int const initial_size = 20; // un sector
	int const final_size = 1024; // mucho mas que un sector

	char* src_buffer = new char[final_size];
	char* dst_buffer = new char[initial_size+1];

	for (int i = 0; i < final_size; ++i) {
		src_buffer[i] = 'a' + (i % 26);
	}

    ASSERT(fileSystem->Create("pepe", initial_size));

    OpenFile *file = fileSystem->Open("pepe");
    ASSERT(file);
    
    file->Write(src_buffer, initial_size);

    file->Seek(0);
    file->Read(dst_buffer, initial_size);
	dst_buffer[initial_size] = '\0';

    printf("Read %s\n", dst_buffer);
    
	file->Seek(0);
	file->Write(src_buffer, final_size);

	file->Seek(final_size - initial_size);
    file->Read(dst_buffer, initial_size);
	dst_buffer[initial_size] = '\0';

    printf("Read %s\n", dst_buffer);

    currentThread->Finish();
}
