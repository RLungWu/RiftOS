# RiftOS Agent Instructions

## Project context

RiftOS is a teaching operating system targeting RV64 on QEMU's `virt` machine.
The kernel runs in supervisor mode and is written in C with a small amount of
RISC-V assembly. The project is implemented one milestone at a time so that
each subsystem remains runnable and explainable.

## Current milestone

M0 and M1 have been completed and accepted by the user.

- M0: reproducible build and boot message in QEMU.
- M1: UART driver and `panic()` path. See
  `docs/milestones/M1-uart-console-and-panic.md` for the data flow, the panic
  invariant, and the known limitations.

The next milestone is M2:

> Trap handler and timer interrupt — how does control leave normal kernel code
> and return?

M2 has not started. Before writing any M2 code, agree on its work sequence
with the user and record it in this file as small, reviewable slices. Do not
start memory allocation, virtual memory, scheduling, or user mode as part of
M2.

Known M1 limitations that M2 must keep in mind:

- `panic()` does not disable interrupts. Once interrupts are enabled, a trap
  could resume execution after a panic, which would break the panic invariant.
- `panic()` depends on the UART driver to report anything.

## Build and run

```sh
make clean && make run                # normal boot
make clean && make run PANIC_TEST=1   # panic test (-DRIFTOS_PANIC_TEST)
```

The makefile does not track `CFLAGS` or header dependencies, so run
`make clean` when switching `PANIC_TEST` or after editing a header. Exit QEMU
with `Ctrl-A` then `X`.

Expected output:

```text
RiftOS booted                 # normal boot
RiftOS booted                 # PANIC_TEST=1
[PANIC]: Test
```

Both checks must keep passing in later milestones.

## Code layout conventions

- `include/`: public headers. Expose APIs only; keep register details in the
  `.c` file.
- `kernel/drivers/`: code that touches device registers directly.
- `kernel/`: kernel code that uses driver APIs (for example `main.c`,
  `panic.c`).
- `compile_flags.txt`: clangd flags for the RISC-V target. Keep it in sync with
  `CFLAGS` in the makefile.

## Learning workflow

- Prefer explaining the rationale and expected result over writing the core
  subsystem code for the user.
- Leave the user room to implement the focused slice.
- Break each slice into very small steps, one change at a time, and verify
  each step in QEMU before moving on. Separate pure moves or renames from
  behavior changes.
- Explain in Traditional Chinese. Use concrete evidence (QEMU output,
  `objdump`, `nm`) to show why something works or fails.
- Review the user's diff and QEMU output after each slice.
- Ask one focused conceptual question at a time.
- Keep changes narrow and avoid unrelated cleanup.
