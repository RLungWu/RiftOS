#include <stdint.h>

#include "sbi.h"
#include "panic.h"
#include "timer.h"
#include "uart.h"

#define TIME_INTERVAL  100000
#define TICKS_PER_PRINT  100

static uint64_t ticks;
static uint64_t next_deadline;

void timer_init(void){
    uint64_t time;

    __asm__ volatile("csrr %0, time" : "=r"(time));
    next_deadline = time + TIME_INTERVAL;
    if (sbi_set_timer(time + TIME_INTERVAL) != 0) {
        panic("sbi_set_timer failed");
    }
    __asm__ volatile("csrs sie, %0" : : "r"(1UL << 5));
    __asm__ volatile("csrs sstatus,%0" : : "r"(2));
}

void timer_interrupt(void){
    ticks += 1;
    next_deadline += TIME_INTERVAL;
    sbi_set_timer(next_deadline);
    if(ticks % TICKS_PER_PRINT == 0){
        uart_puts("Tick:");
        uart_puthex(ticks);
        uart_puts("\r\n");
    }
}

