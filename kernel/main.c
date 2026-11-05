#include "uart.h"

void kernel_main(void){
    uart_puts("RiftOS booted\r\n");

    for (;;) {
        __asm__ volatile("wfi");
    }
}
