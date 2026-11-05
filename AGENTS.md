# RiftOS Agent Instructions

## Project context

RiftOS is a teaching operating system targeting RV64 on QEMU's `virt` machine.
The kernel runs in supervisor mode and is written in C with a small amount of
RISC-V assembly. The project is implemented one milestone at a time so that
each subsystem remains runnable and explainable.

## Current milestone

M0 has been completed and accepted by the user. We are now implementing M1:

> UART console and panic path — how does the kernel report state when it
> cannot continue?

M1 is complete only when the UART output and panic behavior work in QEMU, and
the data flow, invariant, and known limitation are documented.

## M1 work sequence

Work in small, reviewable slices:

1. Extract the current raw UART writes into a small UART driver.
2. Add the `panic()` path that prints a diagnostic message and never returns.
3. Add the M1 milestone documentation and run the QEMU acceptance checks.

Do not start trap handling, timer interrupts, memory allocation, virtual
memory, scheduling, or user mode as part of M1.

## Current task: UART driver

The next implementation slice is:

- Add `include/uart.h`.
- Add `kernel/drivers/uart.c`.
- Move `uart_putc()` and `uart_puts()` out of `kernel/main.c`.
- Keep `kernel/main.c` dependent on the UART API rather than UART register
  details.
- Use QEMU `virt`'s 16550-compatible UART at `0x10000000`.
- `uart_putc()` must wait for the transmitter holding register to be empty
  before writing the next byte.
- Use `volatile` for MMIO register access.

Relevant registers:

```c
#define UART0_BASE 0x10000000UL
#define UART_THR 0
#define UART_LSR 5
#define UART_LSR_THRE (1 << 5)
```

The expected behavior must remain:

```text
RiftOS booted
```

Do not implement `panic()` in this slice. Review the user's implementation
before moving to the next slice.

## Learning workflow

- Prefer explaining the rationale and expected result over writing the core
  subsystem code for the user.
- Leave the user room to implement the focused slice.
- Review the user's diff and QEMU output after each slice.
- Ask one focused conceptual question at a time.
- Keep changes narrow and avoid unrelated cleanup.

## M1 acceptance behavior

Normal boot must print:

```text
RiftOS booted
```

The later panic test must print a `[PANIC]` diagnostic and stop permanently;
execution must not return to the caller or continue into later kernel code.

The planned documentation file is:

```text
docs/milestones/M1-uart-console-and-panic.md
```

It should record the data flow `_start -> kernel_main -> UART API -> UART
MMIO`, the panic invariant, and the remaining limitations.
