#include <stdint.h>

#include "uart.h"

#define UART0_BASE 0x10000000UL
#define UART_THR 0
#define UART_LSR 5
#define UART_LSR_THRE (1 << 5)

void uart_putc(char c){
    while((*(volatile uint8_t *)(UART0_BASE + UART_LSR) & UART_LSR_THRE) == 0){
        // Wait until THR is empty
    }
    *(volatile uint8_t *)(UART0_BASE + UART_THR) = (uint8_t) c;
}

void uart_puts(const char *message){
    while (*message != '\0'){
        uart_putc(*message);
        message++;
    }
}