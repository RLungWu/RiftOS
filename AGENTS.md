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

M2 is in progress. M2-0 and M2-1 are done; M2-2 is next. Do not start memory
allocation, virtual memory, scheduling, or user mode as part of M2.

The user's study notes for M2-1 are in `docs/notes/M2-1-trap-basics.md`.

### M2 work sequence (agreed with the user)

Do the slices in order. Each slice must run in QEMU and be reviewed before the
next one starts.

| Slice | Goal | Verification |
| --- | --- | --- |
| M2-0 | Add `uart_puthex(uint64_t)` so trap CSRs can be printed. API only, no behavior change. | `main` prints a known constant (for example `0x80200000`) correctly. |
| M2-1 | First trap, no return: an assembly trap entry (4-byte aligned) installed with `csrw stvec`. Under a new `TRAP_TEST` flag, run `ebreak`. The C handler prints `scause`, `sepc`, and `stval`, then calls `panic`. | `scause = 3` (breakpoint). `sepc` equals the address of `ebreak` in `objdump`. |
| M2-2 | Return from the trap: save and restore the general registers in a trap frame on the stack, advance `sepc` past `ebreak`, then `sret`. | The message after `ebreak` prints. `objdump` shows whether `ebreak` is 2 bytes (`c.ebreak`) or 4 bytes, which decides how far `sepc` advances. |
| M2-3 | Restore the panic invariant: `panic()` clears `sstatus.SIE` first. Must land before any slice enables interrupts. | `PANIC_TEST=1` still passes. |
| M2-4 | First timer interrupt: SBI TIME extension `set_timer`, enable `sie.STIE` and `sstatus.SIE`. The handler recognizes interrupt bit + code 5, prints once, then pushes the timer far into the future. | Exactly one timer line prints. |
| M2-5 | Periodic tick: reprogram the next timer on every interrupt, count ticks, print every N ticks. | Tick lines keep appearing. With `PANIC_TEST=1`, no tick prints after `[PANIC]`. |

Open decision: `entry.S` does not zero `.bss`. The M2-5 tick counter lives in
`.bss` and currently relies on QEMU leaving RAM zeroed. Ask the user whether
to add a small "zero `.bss` in `entry.S`" slice before M2-5.

Facts found during M2-1 that M2-2 depends on:

- `ebreak` assembles to the 2-byte `c.ebreak` (`9002`) because `rv64gc`
  enables compressed instructions, so M2-2 advances `sepc` by 2.
- QEMU sets `stval` to the `ebreak` address for a breakpoint trap.
- OpenSBI already sets `stvec` to `_start` before entering the kernel, so a
  trap before `trap_init()` restarts the kernel instead of hanging.

Known M1 limitations that M2 must keep in mind:

- `panic()` does not disable interrupts. Once interrupts are enabled, a trap
  could resume execution after a panic, which would break the panic invariant.
- `panic()` depends on the UART driver to report anything.

## Build and run

```sh
make clean && make run                # normal boot
make clean && make run PANIC_TEST=1   # panic test (-DRIFTOS_PANIC_TEST)
make clean && make run TRAP_TEST=1    # trap test (-DRIFTOS_TRAP_TEST)
```

The makefile does not track `CFLAGS` or header dependencies, so run
`make clean` when switching `PANIC_TEST` or `TRAP_TEST`, or after editing a
header. Exit QEMU with `Ctrl-A` then `X`.

Expected output:

```text
RiftOS booted                 # normal boot
RiftOS booted                 # PANIC_TEST=1
[PANIC]: Test
RiftOS booted                 # TRAP_TEST=1 (as of M2-1)
scause = 0000000000000003
sepc = <address of ebreak in objdump>
stval = <same as sepc>
[PANIC]: unexpected trap
```

The normal and panic checks must keep passing in later milestones. The trap
check's expected output changes in M2-2, when the trap returns.

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
