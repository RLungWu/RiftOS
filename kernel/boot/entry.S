.section .text.entry
.globl _start

_start:
    # TODO: 把 sp 設定成 boot_stack_top
    la sp, boot_stack_top
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