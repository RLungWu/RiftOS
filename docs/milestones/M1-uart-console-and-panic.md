# M1: UART console and panic path

## Goal

How does the kernel report state when it cannot continue?

M1 gives the kernel two things: a small UART driver for console output, and a
`panic()` path that reports a fatal condition and stops the hart for good.

## Data flow

```text
_start  ->  kernel_main  ->  UART API  ->  UART MMIO
entry.S     main.c           uart.h        uart.c -> 0x10000000
```

| Layer | File | Responsibility | Knows about |
| --- | --- | --- | --- |
| `_start` | `kernel/boot/entry.S` | Set `sp` to `boot_stack_top` and call `kernel_main`. Falls into a `wfi` loop if `kernel_main` ever returns. | The stack symbol and `kernel_main` |
| `kernel_main` | `kernel/main.c` | Print the boot message, then idle in a `wfi` loop. | Only the UART API and `panic()` |
| UART API | `include/uart.h` | `uart_putc(char)` sends one byte; `uart_puts(const char *)` sends a NUL-terminated string. | Nothing about hardware |
| UART MMIO | `kernel/drivers/uart.c` | Talk to QEMU `virt`'s 16550-compatible UART at `0x10000000`. | Register offsets and bits |

`panic()` (`include/panic.h`, `kernel/panic.c`) sits next to `kernel_main`: it
uses the UART API and has no knowledge of UART registers.

### UART transmit

`uart_putc()` follows the 16550 transmit protocol:

1. Read LSR (`base + 5`) until bit 5, THRE (Transmitter Holding Register
   Empty), is 1.
2. Write the byte to THR (`base + 0`).

THR holds a single byte. The UART shifts it out far more slowly than the CPU
can write, so writing without waiting for THRE can overwrite a byte that has
not been sent yet.

Both registers are accessed through `volatile uint8_t *`. LSR is changed by
the device, not by the program, so every read must reach the hardware.
Without `volatile`, GCC at `-O2` reads LSR once and, if THRE is 0, turns the
wait loop into a jump to itself (verified with `objdump`).

## Panic invariant

> `panic()` prints a `[PANIC]` diagnostic and never returns. No code after a
> call to `panic()` runs.

How the invariant is enforced and checked:

- `panic()` prints its message once, then enters `for (;;) { wfi; }`. There is
  no path that reaches the end of the function.
- `panic()` is declared `__attribute__((noreturn))`. GCC warns if any path can
  return, and the caller relies on the promise: in the panic test build,
  `kernel_main` ends at `jal panic` and the code after the call is removed.
- The panic test places `uart_puts("Should not print\r\n")` directly after
  `panic("Test")`. The acceptance check confirms that line never appears.

## Acceptance

Normal boot:

```sh
make clean && make run
```

```text
RiftOS booted
```

Panic test (enables `RIFTOS_PANIC_TEST` through `-D`):

```sh
make clean && make run PANIC_TEST=1
```

```text
RiftOS booted
[PANIC]: Test
```

After the panic line, the console stays silent and `Should not print` never
appears. Exit QEMU with `Ctrl-A` then `X`.

## Known limitations

1. **Panic depends on the UART.** If the UART is broken (for example THRE
   never becomes 1), `panic()` hangs in `uart_putc()` without printing
   anything. If `uart_putc()` ever called `panic()`, the two would recurse.
2. **QEMU cannot exercise the THRE wait.** QEMU's UART transmits instantly, so
   THRE always reads 1 and the wait loop never iterates. A missing `volatile`
   or a wrong wait condition would not show up in QEMU; it is checked only by
   reading the code and the `objdump` output.
3. **The build does not track flags or headers.** `make` compares only `.c`
   and `.o` timestamps. Changing `PANIC_TEST` or editing a header does not
   trigger a rebuild, so run `make clean` first.
4. **The UART is not initialized.** Baud rate and line format come from QEMU
   and OpenSBI defaults. Real hardware may need explicit setup.
5. **No formatted output.** `panic()` accepts a plain string only. It cannot
   print numbers, addresses, or register values yet.
6. **Panic does not disable interrupts.** M1 never enables interrupts, so this
   is safe for now. Once M2 enables the timer, `panic()` must disable
   interrupts first, or a trap could resume execution after a panic.
7. **Single hart only.** If several harts printed or panicked at the same time,
   their output would interleave.
