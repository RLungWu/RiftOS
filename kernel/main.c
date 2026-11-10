#include <stdint.h>

#include "uart.h"
#include "panic.h"
#include "trap.h"

void kernel_main(void){
    uart_puts("RiftOS booted\r\n");
    trap_init();
    
    #ifdef RIFTOS_TRAP_TEST
        __asm__ volatile("ebreak");
        uart_puts("Should not print\r\n");
    #endif

    #ifdef RIFTOS_PANIC_TEST
        panic("Test");
        uart_puts("Should not print\r\n");
    #endif


    for (;;) {
        __asm__ volatile("wfi");
    }
}
