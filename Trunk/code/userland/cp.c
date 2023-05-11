#include "syscall.h"

int main(int argc, char **argv) {
    if (argc != 3) {
        return -1;
    }

    const char errorMsg[] = "Error: could not create target\n";

    char *source = *(argv+1);
    char *target = *(argv+2);

    OpenFileId sourceFd = Open(source);
    if (sourceFd == -1) {
        return 0;
    }

    if (Create(target) == -1) {
        Write(errorMsg, sizeof(errorMsg)-1, CONSOLE_OUTPUT);
        Close(sourceFd);
        return 0;
    }

    OpenFileId targetFd = Open(target);
    const unsigned bufferSize = 128;
    char buffer[bufferSize];

    unsigned bytesRead;
    while ((bytesRead = Read(buffer, bufferSize, sourceFd))) {
        Write(buffer, bytesRead, targetFd);
    }

    Close(sourceFd);
    Close(targetFd);
    return 0;
}
