#include "defs.h"

#include <stdarg.h>

sigset_t
gs() {
    sigset_t s;
    pthread_sigmask(SIG_SETMASK, 0, &s);
    return s;
}


void write_char(char c) {
    write(STDOUT_FILENO, &c, 1);
}

void write_str(const char *str) {
    while (*str) {
        write_char(*str++);
    }
}

void write_hex(unsigned long value) {
    char buffer[16];  // Assuming a long integer, so 16 characters plus null terminator
    int length = 0;

    // Convert long integer to hexadecimal string
    do {
        buffer[length++] = "0123456789abcdef"[value % 16];
        value /= 16;
    } while (value);

    // Reverse the string
    for (int i = length - 1; i >= 0; --i) {
        write_char(buffer[i]);
    }
}

void write_int(int value) {
    if (value < 0) {
        write_char('-');
        value = -value;
    }
    write_hex(value);
}

void write_unsigned(unsigned int value) {
    write_hex(value);
}

void write_ptr(void *ptr) {
    write_hex((unsigned long)ptr);
}

void dbg_printf(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1) != '\0') {
            switch (*(++fmt)) {
                case 'd':
                    write_int(va_arg(args, int));
                    break;
                case 's':
                    write_str(va_arg(args, const char *));
                    break;
                case 'x':
                    write_hex(va_arg(args, unsigned int));
                    break;
                case 'l':
                    if (*(fmt + 1) == 'x') {
                        write_hex(va_arg(args, unsigned long));
                        fmt++;
                    } else if (*(fmt + 1) == 'u') {
                        write_hex(va_arg(args, unsigned long));
                        fmt++;
                    } else if (*(fmt + 1) == 'd') {
                        write_int(va_arg(args, int));
                        fmt++;
                    } else {
                        write_char('l');
                    }
                    break;
                case 'u':
                    if (*(fmt + 1) == 'l') {
                        write_unsigned(va_arg(args, unsigned long));
                        fmt++;
                    } else {
                        write_unsigned(va_arg(args, unsigned int));
                    }
                    break;
                case 'p':
                    write_ptr(va_arg(args, void *));
                    break;
                default:
                    write_char(*fmt);
            }
        } else {
            write_char(*fmt);
        }
        fmt++;
    }
    va_end(args);
}