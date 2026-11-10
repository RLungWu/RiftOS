#include "uart.h"
#include "panic.h"

__attribute__((noreturn)) void panic(const char *msg){
    __asm__ volatile ("csrci sstatus, 2");
    uart_puts("[PANIC]: ");
    uart_puts(msg);
    uart_puts("\r\n");
    
    for (;;) {
        __asm__ volatile("wfi");
    }
}