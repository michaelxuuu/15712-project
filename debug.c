#include "defs.h"

#include <stdarg.h>

sigset_t
debug_get_mask() {
    sigset_t s;
    pthread_sigmask(SIG_SETMASK, 0, &s);
    return s;
}

void
debug_get_stat() {
    struct tcb *running = 0;
    int total_cnt = 0;
    int alive_cnt = 0;
    for (struct tcb *p = mycore->thrs.next; p; p = p->next, total_cnt++) {
        if (p->state != JOINABLE)
            alive_cnt++;
        if (p->state == RUNNING)
            running = p;
    }
    debug_printf("total: %d alive: %d", total_cnt, alive_cnt);
}

void write_char(char c) {
    write(STDERR_FILENO, &c, 1);  // File descriptor 1 is stdout
}

void write_str(const char *str) {
    while (*str) {
        write_char(*str++);
    }
}

void write_int(int value) {
    char buffer[20];  // Assuming a 32-bit integer, which can have up to 10 digits
    int i = 0;

    if (value < 0) {
        write_char('-');
        value = -value;
    }

    // Convert the integer to a string
    do {
        buffer[i++] = '0' + value % 10;
        value /= 10;
    } while (value > 0);

    // Reverse the string
    while (i > 0) {
        write_char(buffer[--i]);
    }
}

void write_hex(unsigned long value, int uppercase) {
    char buffer[20];
    int i = 0;

    do {
        char digit = value % 16;
        buffer[i++] = digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + digit - 10;
        value /= 16;
    } while (value > 0);

    while (i > 0) {
        write_char(buffer[--i]);
    }
}

void
debug_printf(char *format, ...) {
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd':
                    write_int(va_arg(args, int));
                    break;
                case 'c':
                    write_char(va_arg(args, int));
                    break;
                case 's':
                    write_str(va_arg(args, const char *));
                    break;
                case 'x':
                    write_hex(va_arg(args, unsigned int), 0);
                    break;
                case 'X':
                    write_hex(va_arg(args, unsigned int), 1);
                    break;
                case 'u':
                    write_int(va_arg(args, unsigned int));
                    break;
                case 'p':
                    write_str("0x");
                    write_hex((unsigned long)va_arg(args, void *), 1);
                    break;
                case 'l':
                    format++;
                    switch (*format) {
                        case 'd':
                            write_int(va_arg(args, long));
                            break;
                        case 'x':
                            write_hex(va_arg(args, unsigned long), 0);
                            break;
                        case 'u':
                            write_int(va_arg(args, unsigned long));
                            break;
                        default:
                            write_char('%');
                            write_char('l');
                            format--;  // Move back to the last character
                    }
                    break;
                default:
                    write_char(*format);
            }
        } else {
            write_char(*format);
        }
        format++;
    }

    va_end(args);
}
