# RiftOS Agent Instructions

## Project context

RiftOS is a teaching operating system targeting RV64 on QEMU's `virt` machine.
The kernel runs in supervisor mode and is written in C with a small amount of
RISC-V assembly. The project is implemented one milestone at a time so that
each subsystem remains runnable and explainable.

## Current milestone

M0, M1, and M2 have been completed and accepted by the user.

- M0: reproducible build and boot message in QEMU.
- M1: UART driver and `panic()` path. See
  `docs/milestones/M1-uart-console-and-panic.md` for the data flow, the panic
  invariant, and the known limitations.
- M2: trap handler and timer interrupt ("how does control leave normal kernel
  code and return?"). See `docs/milestones/M2-trap-and-timer.md` for the data
  flow, invariants, acceptance checks, pitfalls, and known limitations.

The next milestone has not been chosen. Before writing any code for it, agree
on its goal and its work sequence with the user and record them in this file
as small, reviewable slices, as was done for M2 below. Read the M2 known
limitations first; several of them (the trap frame not visible to C, the
single 4 KiB stack, the unchecked `sbi_set_timer` result in
`timer_interrupt`) are likely to matter.

The user's study notes are in `docs/notes/` (`M2-1-trap-basics.md`,
`M2-2-trap-return.md`).

### M2 work sequence (completed, kept for reference)

Each slice ran in QEMU and was reviewed before the next one started.

| Slice | Goal | Verification |
| --- | --- | --- |
| M2-0 | Add `uart_puthex(uint64_t)` so trap CSRs can be printed. API only, no behavior change. | `main` prints a known constant (for example `0x80200000`) correctly. |
| M2-1 | First trap, no return: an assembly trap entry (4-byte aligned) installed with `csrw stvec`. Under a new `TRAP_TEST` flag, run `ebreak`. The C handler prints `scause`, `sepc`, and `stval`, then calls `panic`. | `scause = 3` (breakpoint). `sepc` equals the address of `ebreak` in `objdump`. |
| M2-2 | Return from the trap: save and restore the general registers in a trap frame on the stack, advance `sepc` past `ebreak`, then `sret`. | The message after `ebreak` prints. `objdump` shows whether `ebreak` is 2 bytes (`c.ebreak`) or 4 bytes, which decides how far `sepc` advances. |
| M2-3 | Restore the panic invariant: `panic()` clears `sstatus.SIE` first. Must land before any slice enables interrupts. | `PANIC_TEST=1` still passes. |
| M2-4 | First timer interrupt: SBI TIME extension `set_timer`, enable `sie.STIE` and `sstatus.SIE`. The handler recognizes interrupt bit + code 5, prints once, then pushes the timer far into the future. | Exactly one timer line prints. |
| M2-5 | Zero `.bss` before C runs: `linker.ld` exports `.bss` start and end symbols, and `entry.S` stores zero over that range before `call kernel_main`. | `nm` symbols match the `.bss` range in `objdump -h`. `objdump` shows the loop before `call kernel_main`. A static variable reads 0 even when `.bss` is filled with garbage first. All three checks still pass. |
| M2-6 | Periodic tick: reprogram the next timer on every interrupt, count ticks, print every N ticks. | Tick lines keep appearing. With `PANIC_TEST=1`, no tick prints after `[PANIC]`. |

M2-5 (zero `.bss`) was added before the periodic tick. The ELF stores no
bytes for `.bss` (`ALLOC` only; `FileSiz 0`), and QEMU's `-kernel` ELF loader
happens to zero-fill it. A raw binary loaded by a bootloader, or a warm
reboot, would leave garbage, so `entry.S` now clears `__bss_start` to
`__bss_end` before `call kernel_main`.

Facts found during M2:

- `ebreak` assembles to the 2-byte `c.ebreak` (`9002`) because `rv64gc`
  enables compressed instructions, so `trap_handler` advances `sepc` by 2.
  This is hard-coded and only valid for `c.ebreak`; any other exception
  panics.
- QEMU sets `stval` to the `ebreak` address for a breakpoint trap.
- OpenSBI already sets `stvec` to `_start` before entering the kernel, so a
  trap before `trap_init()` restarts the kernel instead of hanging.
- `trap_entry` builds a 256-byte trap frame on the current stack and stores
  `xN` at offset `N * 8` (x1, x3–x31; x0 and sp are not stored). Without it,
  `a0` came back from the trap as `0x802001EA` (the end of the `"\r\n"` string
  `trap_handler` printed last) instead of `0x1234`.
- `panic()` runs `csrci sstatus, 2` (clear SIE only) before its first
  `uart_puts`. `objdump` shows it right after the 4-instruction prologue,
  which is safe because nothing has been printed yet. `csrw sstatus, 0` was
  rejected: boot-time `sstatus` is `0x8000000200006000`, and clearing FS
  (bits 13–14) would make any floating-point instruction trap. M2-4 showed it
  working: with `PANIC_TEST=1` the 1-second timer never prints after
  `[PANIC]`.
- QEMU `virt` `timebase-frequency` is 10 MHz (`0x989680` ticks per second,
  from the device tree). By default `time` follows host wall-clock time, so
  gaps measured in `time` depend on host speed; `-icount shift=0` makes them
  deterministic.
- `sbi_set_timer()` (`kernel/sbi.c`) calls SBI TIME (`a7 = 0x54494D45`,
  `a6 = 0`, `a0 = stime_value`). It declares `a1` as an output because the
  SBI returns a value there. `medeleg` bit 9 is 0, so S-mode `ecall` goes to
  OpenSBI.
- At boot `sip = 0x20` (STIP already pending) because `stimecmp = 0`. Interrupts
  must be enabled only after `sbi_set_timer()`, or a stale interrupt fires
  first and the timer line prints twice. `kernel_main` enables `sie.STIE`
  with `csrs sie, %0` (the 5-bit `csrsi` immediate cannot hold 32), then
  `sstatus.SIE`.
- The timer branch in `trap_handler` compares the full `scause`
  (`0x8000000000000005`); exception code 5 without bit 63 is a load access
  fault. It does not touch `sepc`: for an interrupt, `sepc` is the next
  instruction not yet run (the second `wfi` of the unrolled idle loop). It
  stops the timer with `sbi_set_timer(~0UL)`.
- Timer code lives in `kernel/timer.c` (`timer_init`, `timer_interrupt`).
  `trap_handler` only classifies the trap: it checks the timer `scause` first
  and returns without printing, and prints `scause`, `sepc`, and `stval` only
  for `ebreak` and unexpected traps. An earlier attempt hid those prints
  behind `#ifdef RIFTOS_PANIC_TEST`, which would have removed the diagnostics
  from unexpected traps in normal builds.
- The tick is 10 ms (`100000` ticks of `time`), printed every 100 ticks
  (about once per second). The next deadline is `next_deadline +=
  interval` rather than `time + interval`, so handler latency does not
  accumulate as drift. If the handler ever runs longer than one interval,
  the new deadline is already past and the interrupt fires again at once to
  catch up.
- `ticks` and `next_deadline` are file-scope `static` variables in `.sbss`,
  inside the cleared range. Only `timer_interrupt` writes them, and it runs
  with SIE cleared, so there is no concurrent update on this single hart.
- RISC-V GCC places small globals (8 bytes or less) in `.sbss`, not `.bss`,
  for `gp`-relative access. Before `*(.sbss .sbss.*)` was added to the `.bss`
  output section, a `static volatile uint64_t` probe landed at `__bss_end`,
  outside the cleared range. With it, a test that first fills `.bss` with
  `-1` prints `FFFFFFFFFFFFFFFF` without the clear loop and `0` with it. The
  M2-6 tick counter would also land in `.sbss`.
- The clear loop runs before any stack use, so clearing `boot_stack` is safe.
  It must load `t0`/`t1` itself; once they were accidentally deleted, and
  only `objdump` showed the loop using leftover OpenSBI register values.

Known M1 limitation that later milestones must keep in mind:

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
Tick:0000000000000064         # about once per second, +0x64 each line
Tick:00000000000000C8
Tick:000000000000012C

RiftOS booted                 # PANIC_TEST=1
[PANIC]: Test                 # no tick after this, ever

RiftOS booted                 # TRAP_TEST=1
scause = 0000000000000003
sepc = <address of ebreak in objdump>
stval = <same as sepc>
a0 after trap = 0000000000001234
Tick:0000000000000064         # then ticks as in the normal boot
Tick:00000000000000C8
```

All three checks must keep passing in later milestones. Watch each run for
at least 3 seconds so several tick lines appear.

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
