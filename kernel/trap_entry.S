.section .text
.align 2
.globl trap_entry

trap_entry:
    call trap_handler

1:
    wfi
    j 1b

