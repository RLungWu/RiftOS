# RiftOS

RiftOS is a small teaching operating system built from scratch for a RISC-V
machine emulated by QEMU. The goal is to understand how a kernel works from
its first instruction to a user program and a shell.

This is a learning project, not a production operating system. Each milestone
must leave behind a runnable system, a small amount of documentation, and a
clear invariant that can be checked.

## Target and host

The development host is an Apple Silicon MacBook Air M5. The host CPU is not
the CPU being implemented: RiftOS targets `RV64` and runs inside QEMU's
`virt` machine.

The initial platform is deliberately fixed:

- Target: 64-bit RISC-V (`RV64`)
- Board: QEMU `virt`
- Firmware: OpenSBI
- Kernel privilege level: supervisor mode (S-mode)
- Initial CPU model: one hart
- Kernel language: C with a small amount of RISC-V assembly
- First user interface: UART console

Using an emulated RISC-V target keeps the project reproducible on the M5 Mac
and makes the machine state observable while learning. Native macOS code is
not part of the kernel execution path.

## Scope

The first complete version will include:

1. Boot and kernel entry
2. Traps and timer interrupts
3. Physical page allocation
4. Sv39 virtual memory
5. Kernel threads and context switching
6. Timer-based round-robin scheduling
7. User mode and system calls
8. ELF loading and process lifecycle
9. A small filesystem and interactive shell

The first version will not include networking, graphics, USB, GPU support,
SMP, containers, or POSIX completeness. Those features can be considered only
after the nine milestones are complete and understood.

## Milestones

| Milestone | Result | Main question |
| --- | --- | --- |
| M0 | Reproducible build and `Hello kernel` in QEMU | Where does the first instruction come from? |
| M1 | UART console and panic path | How does the kernel report state when it cannot continue? |
| M2 | Trap handler and timer interrupt | How does control leave normal kernel code and return? |
| M3 | Physical page allocator | Which pages are free, allocated, and safe to reuse? |
| M4 | Sv39 page tables and page faults | How does a virtual address become a physical address? |
| M5 | Kernel threads and context switching | Which registers and stack state define an execution context? |
| M6 | Preemptive round-robin scheduler | How can an infinite loop be prevented from owning the CPU? |
| M7 | User mode and basic syscalls | How is kernel memory protected from user code? |
| M8 | ELF loading and process lifecycle | How does a program become a running process and then exit? |
| M9 | Ram filesystem and shell | How do storage, programs, and user interaction fit together? |

We will implement and review one milestone at a time. A milestone is complete
only when its acceptance behavior works in QEMU and its important invariant is
written down.

## Repository layout

The layout will grow with the milestones:

```text
RiftOS/
├── kernel/
│   ├── boot/
│   ├── trap/
│   ├── mm/
│   ├── sched/
│   ├── proc/
│   ├── fs/
│   └── drivers/
├── user/
├── include/
├── docs/
│   └── milestones/
├── tests/
├── Makefile
└── README.md
```

Directories are added when the corresponding subsystem is introduced; the
repository will not contain empty scaffolding just for appearance.

## Development workflow

Each milestone follows the same loop:

1. Read the relevant RISC-V or QEMU documentation.
2. Add the smallest runnable implementation.
3. Run the milestone's acceptance check in QEMU.
4. Inspect failures with serial output and GDB when needed.
5. Record the data flow, invariant, and one remaining limitation in `docs/`.

Planned commands are:

```sh
make            # build the kernel
make run        # boot RiftOS in QEMU
make test       # run deterministic host-side checks where applicable
```

The exact cross-compiler and QEMU setup will be checked and documented in M0
for macOS on Apple Silicon. Build instructions must not assume that the host
compiler can emit RISC-V binaries.

## Current status

The project is at the planning stage. The next step is M0: verify the RISC-V
toolchain and QEMU installation, then boot the smallest possible kernel and
print a startup message over UART.

## Learning rule

RiftOS is implemented incrementally. The assistant may explain concepts,
provide a narrow skeleton, and review changes, but the core subsystem code is
written and debugged as part of the learning process. Every milestone should
leave enough evidence to explain not only that it works, but why it works.
