#include "syscall.h"

int main(int argc, char **argv) {
    const unsigned bufferSize = 128;
    for (argv++, argc--; argc > 0; argv++, argc--) {
        char *filename = *argv;
        char buffer[bufferSize];

        OpenFileId fd = Open(filename);
        if (fd == -1) { // if one of the arguments doesnt match a file, ignore
                        // it.
            continue;
        }

        unsigned bytesRead;
        while ((bytesRead = Read(buffer, bufferSize, fd))) {
            Write(buffer, bytesRead, CONSOLE_OUTPUT);
        }

        Close(fd);
    }

    return 0;
}
