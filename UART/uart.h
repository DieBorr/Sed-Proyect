#ifndef UART_H
#define UART_H

#include <stdlib.h>

#define ERR   (-1)
#define OK    (0)

int uart0_init(int baudrate);

// Non-blocking functions:
int uart0_send(const void *buf, size_t len);
int uart0_recv(void *buf, size_t len);

int uart0_kbhit(void);

// String & character I/O blocking functions:
char *uart0_gets(char *s);
int uart0_getchar(void);
int uart0_fputs(const char *s);
int uart0_puts(const char *s);


#endif
