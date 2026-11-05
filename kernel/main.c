#include "uart.h"
#include "panic.h"

void kernel_main(void){
    uart_puts("RiftOS booted\r\n");
    
    #ifdef RIFTOS_PANIC_TEST
        panic("Test");
        uart_puts("Should not print\r\n");
    #endif


    for (;;) {
        __asm__ volatile("wfi");
    }
}
