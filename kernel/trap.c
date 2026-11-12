#include <stdint.h>

#include "panic.h"
#include "uart.h"
#include "trap.h"
#include "sbi.h"

extern void trap_entry(void);

#define FLAG_FOR_INTERRUPT_IN_TRAP (1UL << 63)
#define S_MODE_CLOCK 5

void trap_handler(void){
    uint64_t scause, sepc, stval;

    __asm__ volatile("csrr %0, scause" : "=r"(scause));
    __asm__ volatile("csrr %0, sepc" : "=r"(sepc));
    __asm__ volatile("csrr %0, stval" : "=r"(stval));

    uart_puts("scause = ");
    uart_puthex(scause);
    uart_puts("\r\n");

    uart_puts("sepc = ");
    uart_puthex(sepc);
    uart_puts("\r\n");

    uart_puts("stval = ");
    uart_puthex(stval);
    uart_puts("\r\n");

    if (scause == (FLAG_FOR_INTERRUPT_IN_TRAP|S_MODE_CLOCK)){
        uart_puts("time interrupt\r\n");
        sbi_set_timer(~0UL);
        return;
    }

    if (scause == 3) {
        sepc += 2;
        __asm__ volatile("csrw sepc, %0" : : "r"(sepc));
        return;
    }

    panic("unexpected trap");
}

void trap_init(void){
    uint64_t value = (uint64_t)trap_entry;

    __asm__ volatile("csrw stvec, %0" : : "r"(value));
}
