#include "syscall.h"

int main() {
    char hello_world[] = "hello world\n";
    Write(hello_world, sizeof(hello_world)-1, 1);
}
