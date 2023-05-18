#include "syscall.h"

int main() {
	int suma = 0;
	for (unsigned i = 0; i < 10; i++) {
		suma += i;
	}
	return suma;
}
