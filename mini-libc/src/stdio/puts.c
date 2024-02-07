#include <stdio.h>
#include <internal/syscall.h>

int puts(const char *str) {
    char *dummy = (char *)str;
    int size = 0;
    // calculate the length of the string in size variable
    for (; *dummy != '\0'; dummy++, size++) {}

    int total_written = 0;
    // write the string to the standard output using system call until all characters are written
    while (total_written < size) {
        total_written += syscall(__NR_write, 1, str + total_written, size - total_written);
    }
    // Anewline character to the output
    syscall(__NR_write, 1, "\n", 1);

    // return the total number of characters written
    return total_written;
}
