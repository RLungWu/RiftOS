# M2: Trap handler and timer interrupt

## Goal

How does control leave normal kernel code and return?

M2 gives the kernel a trap path that can stop whatever is running, handle the
event, and resume the interrupted code with every register intact. It uses
that path for two events: a deliberate `ebreak` (synchronous exception) and a
periodic supervisor timer interrupt (asynchronous).

## Data flow

### Boot

```text
OpenSBI (M-mode)
  -> _start          set sp, zero .bss (including .sbss)       entry.S
  -> kernel_main                                                main.c
       -> trap_init    stvec = trap_entry                       trap.c
       -> timer_init   first deadline, enable sie.STIE, SIE     timer.c
            -> sbi_set_timer -> ecall -> OpenSBI                sbi.c
       -> wfi loop
```

### Trap

```text
event (ebreak or timer)
  hardware: sepc, scause, stval written; SPIE = SIE; SIE = 0
  -> stvec -> trap_entry       save x1, x3-x31 to a 256-byte frame    trap_entry.S
       -> trap_handler          classify by scause                    trap.c
            timer interrupt  -> timer_interrupt                       timer.c
            breakpoint       -> print CSRs, sepc += 2
            anything else    -> print CSRs, panic
       restore registers, sret  (pc = sepc; SIE = SPIE)
```

| Layer | File | Responsibility | Knows about |
| --- | --- | --- | --- |
| `_start` | `kernel/boot/entry.S` | Set `sp`, zero `__bss_start`..`__bss_end`, call `kernel_main`. | Linker symbols |
| Linker script | `kernel/linker.ld` | Place `.bss.stack`, `.sbss`, `.bss`, and `COMMON` between `__bss_start` and `__bss_end`. | Section names |
| `trap_init` | `kernel/trap.c` | Write the address of `trap_entry` to `stvec` (Direct mode). | `trap_entry` |
| `trap_entry` | `kernel/trap_entry.S` | Save and restore general registers around `trap_handler`, then `sret`. | Nothing about trap causes |
| `trap_handler` | `kernel/trap.c` | Read `scause`, `sepc`, `stval`; dispatch on `scause`. | `scause` encoding, `timer_interrupt`, `panic` |
| Timer | `kernel/timer.c`, `include/timer.h` | `timer_init` arms the first deadline and enables interrupts. `timer_interrupt` counts ticks, arms the next deadline, and prints every 100 ticks. | Tick interval, SBI API |
| SBI | `kernel/sbi.c`, `include/sbi.h` | `sbi_set_timer(stime_value)` calls the SBI TIME extension. | SBI calling convention |
| `panic` | `kernel/panic.c` | Clear `sstatus.SIE`, print, stop. | UART API |

### Who handles which trap

OpenSBI decides, through `medeleg`, which exceptions reach S-mode. On this
machine it prints `MEDELEG : 0x0000000000f4b509`:

- Bit 3 (breakpoint) is 1, so `ebreak` goes straight to our `stvec`.
- Bit 9 (environment call from S-mode) is 0, so `ecall` goes to OpenSBI.
  `sbi_set_timer` relies on this.

### Trap frame

`trap_entry` lowers `sp` by 256 bytes and stores `xN` at offset `N * 8`:

| Offset | Register |
| --- | --- |
| 0 | unused (`x0` is always zero) |
| 8 | `x1` (`ra`) |
| 16 | unused (`x2` is `sp`; restored by `addi sp, sp, 256`) |
| 24 .. 248 | `x3` .. `x31` |

The frame lives on whatever stack was in use when the trap happened. Today
that is always the 4 KiB boot stack.

`trap_entry` must be assembly. The first thing it does is save registers, and
a C function would change `sp` and `ra` in its prologue before any C statement
ran. `call trap_handler` also overwrites `ra`, which is why `x1` is saved
first.

### `scause` dispatch

`scause` bit 63 says interrupt (1) or exception (0); the remaining bits are a
code from the RISC-V privileged specification.

| `scause` | Meaning | Handling |
| --- | --- | --- |
| `0x8000000000000005` | Supervisor timer interrupt | `timer_interrupt()`, no output, `sepc` unchanged |
| `0x0000000000000003` | Breakpoint (`ebreak`) | Print CSRs, `sepc += 2`, return |
| anything else | Unexpected | Print CSRs, `panic("unexpected trap")` |

The full value is compared. Exception code 5 without bit 63 is a load access
fault, not a timer.

For an exception, `sepc` points at the faulting instruction itself. `ebreak`
assembles to the 2-byte `c.ebreak` (`9002`) because `rv64gc` enables
compressed instructions, so the handler skips 2 bytes. For an interrupt,
`sepc` points at the next instruction that has not run yet (for example the
second `wfi` of the unrolled idle loop), so the handler must not change it.

### Timer

`time` counts at the `timebase-frequency` from the device tree: 10 MHz
(`0x989680` per second) on QEMU `virt`. By default it follows host wall-clock
time.

S-mode cannot write the timer compare register, so `sbi_set_timer` asks
OpenSBI:

| Register | Value |
| --- | --- |
| `a7` | `0x54494D45` (TIME extension) |
| `a6` | `0` (`set_timer`) |
| `a0` | absolute deadline in `time` ticks; returns the SBI error (0 = success) |
| `a1` | returns a value; declared as an output so the compiler does not keep data there |

Setting a new deadline also clears the pending timer interrupt. Without a new
deadline the interrupt stays pending and fires again right after `sret`.

`timer_init` runs once:

1. `next_deadline = time + 100000` (10 ms).
2. `sbi_set_timer(next_deadline)`; panic if it fails.
3. Set `sie.STIE` (bit 5) with `csrs sie, %0`. The 5-bit `csrsi` immediate
   cannot hold 32.
4. Set `sstatus.SIE` (bit 1).

The order matters. At boot `sip = 0x20`: a timer interrupt is already pending
because `stimecmp` is 0. Enabling interrupts before the first
`sbi_set_timer` would take that stale interrupt first.

`timer_interrupt` runs every 10 ms:

1. `ticks += 1`.
2. `next_deadline += 100000`.
3. `sbi_set_timer(next_deadline)`.
4. Every 100 ticks (about once per second), print `Tick:` and the count.

The next deadline is computed from the previous deadline, not from the current
`time`. Handler latency then delays only the current tick; it does not
accumulate as drift. If one handler ever ran longer than a full interval, the
new deadline would already be past and the interrupt would fire again at once,
catching up.

`ticks` and `next_deadline` are file-scope `static` variables. They live in
`.sbss` (cleared at boot), only `timer.c` can name them, and only
`timer_interrupt` updates them while `SIE` is 0, so nothing else can change
them at the same time on this single hart.

### `.bss` zeroing

The ELF stores no bytes for `.bss` (`ALLOC` only, `FileSiz 0`). QEMU's
`-kernel` ELF loader happens to zero-fill it, but a raw binary or a warm reboot
would not, so `_start` clears it with 8-byte stores before any C code runs.
Clearing `boot_stack` there is safe because nothing has been pushed yet.

RISC-V GCC puts small globals (8 bytes or less) in `.sbss` for `gp`-relative
access. Before `.sbss` was listed in the `.bss` output section, the linker
placed it after `__bss_end`, outside the cleared range.

## Invariants

### Panic invariant (extended from M1)

> `panic()` prints a `[PANIC]` diagnostic and never returns. No code after a
> call to `panic()` runs, including interrupt handlers.

- `panic()` runs `csrci sstatus, 2` before its first `uart_puts`. `objdump`
  shows it right after the 4-instruction prologue, which is safe because
  nothing has been printed yet.
- `csrci` clears only SIE. `csrw sstatus, 0` would also clear FS (boot-time
  `sstatus` is `0x8000000200006000`), and every floating-point instruction
  would then trap.
- Checked by `PANIC_TEST=1`: the 10 ms timer keeps expiring, yet no `Tick:`
  line ever appears after `[PANIC]`.

### Trap transparency

> After `sret`, the interrupted code sees the same general registers it had
> before the trap. Only `sepc` may change, and only for a handled exception.

- Checked by `TRAP_TEST=1`: `li a0, 0x1234; ebreak; mv %0, a0` prints
  `a0 after trap = 0000000000001234`. Without the trap frame it printed
  `0x802001EA`, the end of the `"\r\n"` string `trap_handler` printed last.

### Zeroed `.bss`

> Every byte between `__bss_start` and `__bss_end`, including `.sbss`, is zero
> when `kernel_main` starts.

- Checked once by experiment: fill the range with `-1` before the clear loop
  and print a `static volatile uint64_t` from C. It printed
  `FFFFFFFFFFFFFFFF` without the clear loop and `0` with it. `volatile` was
  needed; otherwise GCC assumes the never-written static is 0 and does not
  read memory.

## Acceptance

```sh
make clean && make run                # normal boot
make clean && make run PANIC_TEST=1   # panic test
make clean && make run TRAP_TEST=1    # trap test
```

Watch each run for at least 3 seconds. Exit QEMU with `Ctrl-A` then `X`.

Normal boot:

```text
RiftOS booted
Tick:0000000000000064
Tick:00000000000000C8
Tick:000000000000012C
```

One `Tick:` line about every second, each 100 (`0x64`) higher than the last.

Panic test:

```text
RiftOS booted
[PANIC]: Test
```

Nothing after the panic line, no matter how long QEMU runs.

Trap test:

```text
RiftOS booted
scause = 0000000000000003
sepc = <address of ebreak in objdump>
stval = <same as sepc>
a0 after trap = 0000000000001234
Tick:0000000000000064
Tick:00000000000000C8
```

`sepc` must match the `ebreak` line in
`riscv64-elf-objdump -d build/kernel.elf | grep ebreak`.

## Pitfalls found during M2

Each of these left the QEMU output looking normal, or nearly so. They were
caught with `objdump`, `nm`, compiler warnings, or a test built to fail first.

| Mistake | Symptom | How it was caught |
| --- | --- | --- |
| `call trap_entry` instead of `call trap_handler` | A trap would loop forever, silently | `objdump`: `jal 80200178 <trap_entry>` |
| `uint64_t value;` never set before `csrw stvec` | `stvec = 0` | `-Wuninitialized`; `objdump`: `li a5,0` |
| Register variables not used as `asm` operands in `sbi_set_timer` | `a7`, `a6` never loaded | `-Wunused-variable`; `objdump` had no `li a7` |
| `sbi_set_timer(scause)` instead of `~0UL` | Worked by accident (about 29,000 years away) | `objdump`: `mv a0,s1` before `j sbi_set_timer` |
| `csrs sstatus` with mask 1 instead of 2 | Timer never fired | `objdump`: `li t0,1` |
| `.sbss` missing from `linker.ld` | Small statics not cleared | Fill-with-garbage experiment; `nm` showed the variable at `__bss_end` |
| `la t0` / `la t1` deleted from the clear loop | Loop used leftover OpenSBI registers | `objdump` of `_start` |
| CSR prints hidden behind `#ifdef RIFTOS_PANIC_TEST` | Unexpected traps would panic without `scause`/`sepc`/`stval` | `TRAP_TEST` output was missing the breakpoint lines |

## Known limitations

1. **`sepc += 2` is hard-coded for `c.ebreak`.** A 4-byte `ebreak` or any other
   exception that should resume would need the real instruction length. Today
   every other exception panics.
2. **The trap frame is not visible to C.** `trap_handler` takes no arguments,
   so it cannot read or change the saved registers. Later work such as system
   calls will need to pass the frame pointer in `a0`.
3. **One 4 KiB stack for everything.** `kernel_main`, the trap frame
   (256 bytes), and `trap_handler`'s own frames share the boot stack. There is
   no guard page, so an overflow silently overwrites memory below it.
4. **A fault inside the trap handler recurses.** Interrupts are off while the
   handler runs, but an exception (for example a bad pointer) re-enters
   `trap_entry` on the same stack.
5. **Only the supervisor timer interrupt is handled.** Software (code 1) and
   external (code 9) interrupts are not enabled; if one arrived it would panic.
6. **`timer_interrupt` ignores the `sbi_set_timer` return value.** A failure
   would stop the tick without any message.
7. **Printing happens in interrupt context.** QEMU's UART is instant, but at
   115200 baud one `Tick:` line takes about 2 ms of a 10 ms interval.
8. **Tick timing depends on the host.** `time` follows host wall-clock time,
   so gaps measured in ticks vary between runs. `-icount shift=0` makes them
   deterministic.
9. **`.sdata` and `.srodata` are not listed in `linker.ld`.** The linker places
   them as orphan sections. They carry file contents, so they load correctly,
   but they are not covered by any symbol bounds.
10. **Still carried from M1:** `panic()` depends on the UART, the makefile does
    not track headers or `CFLAGS` (run `make clean`), and only one hart is
    supported.

## Commits

M2 was built in slices, each run in QEMU before the next started:

| Slice | Commit | Summary |
| --- | --- | --- |
| M2-0 | `a4e334e` | `uart_puthex` |
| M2-1 | `a7a3293` | `trap_entry`, `stvec`, breakpoint report |
| M2-2 | `de8eb62` | Trap frame, `sepc += 2`, `sret` |
| M2-3 | `e45bcde` | `panic()` clears `sstatus.SIE` |
| M2-4 | `c073b98` | `sbi_set_timer`, first timer interrupt |
| M2-5 | `975f24b` | Zero `.bss` and `.sbss` in `_start` |
| M2-6 | `f4923f8` | `timer.c`, 10 ms periodic tick |
