#ifndef NACHOS_USERLAND_LIB__H
#define NACHOS_USERLAND_LIB__H

/// Get the length of an string.
unsigned strlen(const char *s);

/// Print a string.
///
/// * `s` is the string to be printed.
void puts(const char *s);

/// Get absolute value of an integer.
///
/// * `n` is the integer.
unsigned abs(int n);

/// Convert an integer to a string.
///
/// * `n` is the integer to be converted.
/// * `str` is the place to store the string. It is assumed there is enough
///    space for the result. For an integer the maximum length is 11 (counting
///    the terminator, 12).
void itoa(int n, char *str);

#endif
