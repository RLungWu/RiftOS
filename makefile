CROSS_COMPILE ?= riscv64-elf-
CC             = $(CROSS_COMPILE)gcc
LD             = $(CROSS_COMPILE)ld
QEMU           = qemu-system-riscv64

CFLAGS         = -march=rv64gc -mabi=lp64 -mcmodel=medany \
                 -ffreestanding -nostdlib -O2 -Wall -Iinclude
LDFLAGS        = -T kernel/linker.ld
QEMUFLAGS      = -machine virt -nographic -bios default -kernel

ifeq ($(PANIC_TEST), 1)
CFLAGS		   += -DRIFTOS_PANIC_TEST
endif

ifeq ($(TRAP_TEST), 1)
CFLAGS         += -DRIFTOS_TRAP_TEST
endif

BUILD_DIR      = build
TARGET         = $(BUILD_DIR)/kernel.elf
OBJS           = $(BUILD_DIR)/entry.o \
                 $(BUILD_DIR)/main.o \
                 $(BUILD_DIR)/uart.o \
				 $(BUILD_DIR)/panic.o \
				 $(BUILD_DIR)/trap.o \
				 $(BUILD_DIR)/trap_entry.o \
				 $(BUILD_DIR)/sbi.o

.PHONY: all run clean

all: $(TARGET)


$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/entry.o: kernel/boot/entry.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: kernel/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/uart.o: kernel/drivers/uart.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/panic.o: kernel/panic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/trap.o: kernel/trap.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/trap_entry.o: kernel/trap_entry.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sbi.o: kernel/sbi.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(TARGET)
	$(QEMU) $(QEMUFLAGS) $(TARGET)

clean:
	rm -rf $(BUILD_DIR)