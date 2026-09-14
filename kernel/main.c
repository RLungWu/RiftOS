#include <stdint.h>

#define UART0_BASE 0x10000000UL

static void uart_putc(char c)
{
    *(volatile uint8_t *)UART0_BASE = (uint8_t)c;
}

static void uart_puts(const char *message)
{
    while (*message != '\0') {
        uart_putc(*message);
        message++;
    }
}

void kernel_main(void)
{
    uart_puts("RiftOS booted\r\n");

    for (;;) {
        __asm__ volatile("wfi");
    }
}