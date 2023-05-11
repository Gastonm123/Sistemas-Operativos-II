#include "syscall.h"

int main(int argc, char **argv) {
    for (argc--, argv++; argc > 0; argc--, argv++) {
        Remove(*argv);
    }

    return 0;
}
