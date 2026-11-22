.section .text.entry
.globl _start

_start:
    # TODO: 把 sp 設定成 boot_stack_top
    la sp, boot_stack_top

    la t0, __bss_start
    la t1, __bss_end
_bss_clear_loop:
    bgeu t0, t1, _bss_clear_done
    sd   zero, 0(t0)
    addi t0, t0, 8
    j    _bss_clear_loop
_bss_clear_done:
    # TODO: 呼叫 kernel_main
    call kernel_main

1:
    wfi
    j 1b

.section .bss.stack
.align 12

boot_stack:
    .space 4096

boot_stack_top:
