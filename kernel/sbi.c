#include <stdint.h>

#include "sbi.h"
#include "panic.h"

long sbi_set_timer(uint64_t stime_value){
    register uint64_t a7 __asm__("a7") = 0x54494D45;
    register uint64_t a6 __asm__("a6") = 0;
    register uint64_t a0 __asm__("a0") = stime_value;
    register uint64_t a1 __asm__("a1") = 0;

    __asm__ volatile(
        "ecall"
        : "+r"(a0), "+r"(a1)
        : "r"(a6), "r"(a7)
        : "memory"
    );

    return a0;
}