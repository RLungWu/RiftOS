# 工具鏈與旗標設定
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

# 路徑與檔案
BUILD_DIR      = build
TARGET         = $(BUILD_DIR)/kernel.elf
OBJS           = $(BUILD_DIR)/entry.o \
                 $(BUILD_DIR)/main.o \
                 $(BUILD_DIR)/uart.o \
				 $(BUILD_DIR)/panic.o

# 假想目標（避免與同名檔案衝突）
.PHONY: all run clean

# 預設建置目標
all: $(TARGET)

# 連結成 ELF
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) $^ -o $@

# 組合語言編譯規則
$(BUILD_DIR)/entry.o: kernel/boot/entry.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# C 語言編譯規則
$(BUILD_DIR)/main.o: kernel/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/uart.o: kernel/drivers/uart.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/panic.o: kernel/panic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 自動建立輸出目錄
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 透過 QEMU 執行
run: $(TARGET)
	$(QEMU) $(QEMUFLAGS) $(TARGET)

# 清理編譯產物
clean:
	rm -rf $(BUILD_DIR)