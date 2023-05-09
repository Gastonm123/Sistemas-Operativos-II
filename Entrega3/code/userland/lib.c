#include "syscall.h"

/// Get the length of an string.
unsigned strlen(const char *s) {
    unsigned len;
    for (len = 0; s[len] != '\0'; len++);
    return len;
}

/// Print a string.
///
/// * `s` is the string to be printed.
void puts(const char *s) {
    unsigned len = strlen(s);
    Write(s, len, CONSOLE_OUTPUT);
}

/// Get absolute value of an integer.
///
/// * `n` is the integer.
unsigned abs(int n) {
    if (n > 0) {
        return n;
    }
    return -n;
}

/// Convert an integer to a string.
///
/// * `n` is the integer to be converted.
/// * `str` is the place to store the string. It is assumed there is enough
///    space for the result. For an integer the maximum length is 11 (counting
///    the terminator, 12).
void itoa(int n, char *str) {
    unsigned len = 0;
    unsigned dig = abs(n);
    unsigned i;

    for (; dig > 0; dig /= 10, len++);

    if (n == 0) {
        *str++ = '0';
        *str   = '\0';
    }
    else {
        if (n < 0) {
            *str++ = '-';
        }

        for (i = 1, dig = abs(n); dig > 0; dig /= 10, i++) {
            // dig % 10 is the ith digit from right to left.
            str[len-i] = '0' + (dig % 10);
        }

        str[len] = '\0';
    }
}
