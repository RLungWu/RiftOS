#include <stdint.h>

#include "uart.h"
#include "panic.h"
#include "trap.h"
#include "sbi.h"

void kernel_main(void){
    uart_puts("RiftOS booted\r\n");
    trap_init();
    
    uint64_t time;
    __asm__ volatile("csrr %0, time": "=r"(time));
    if (sbi_set_timer(time + 10000000) != 0) {
        panic("sbi_set_timer failed");
    }
    __asm__ volatile(
        "csrs sie, %0"
        : : "r"(1UL << 5)
    );
    __asm__ volatile(
        "csrs sstatus,%0"
        : : "r"(2)
    );

    #ifdef RIFTOS_TRAP_TEST
        uint64_t v;
        __asm__ volatile(
            "li a0, 0x1234\n"
            "ebreak\n"
            "mv %0, a0"
            : "=r"(v)      // 輸出：%0 → v
            :              // 沒有輸入
            : "a0"         // ← 新東西：clobber 清單
        );
        uart_puts("a0 after trap = ");
        uart_puthex(v);
        uart_puts("\r\n");
    #endif

    #ifdef RIFTOS_PANIC_TEST
        panic("Test");
        uart_puts("Should not print\r\n");
    #endif


    for (;;) {
        __asm__ volatile("wfi");
    }
}
