# M2-1 學習筆記：trap 的基礎

> 進度：M2-1 第 1–4 步完成，第 5 步（`ebreak` 測試）尚未開始。

## 1. 權限等級（privilege mode）

| Mode | 權限 | 在 RiftOS 裡是誰 |
| --- | --- | --- |
| M（Machine） | 最高 | OpenSBI（`-bios default` 載入） |
| S（Supervisor） | 中 | RiftOS kernel |
| U（User） | 最低 | 目前沒有 |

開機順序：QEMU → OpenSBI（M-mode）→ 切到 S-mode → 跳到 `_start`。

## 2. 什麼是 trap

CPU 原本一條一條執行指令，**被某個事件強制打斷，跳到別的位址去處理**，這就叫 trap。

- **例外（exception）**：剛執行的那條指令本身出事，例如 `ebreak`、存取非法位址。
- **中斷（interrupt）**：外部事件，跟目前的指令無關，例如時鐘到點了（M2-4）。

## 3. trap 會跳到 M-mode 還是 S-mode？看 `medeleg`

- 預設：**所有 trap 都送到 M-mode**。
- `medeleg` 是 M-mode 的「委派清單」：bit n = 1 代表編號 n 的例外直接交給 S-mode 處理。
- OpenSBI 開機時印出 `MEDELEG : 0xf4b509`：
  - bit 3 = 1 → breakpoint（`ebreak`）**交給我們的 `stvec`**
  - bit 9 = 0 → S-mode 的 `ecall` **送給 OpenSBI**（M2-4 呼叫 SBI 就是靠這一點）

比喻：OpenSBI 是物業管理處，RiftOS 是住戶。`medeleg` 是公告：「這幾類小事住戶自己處理。」

## 4. trap 相關的 CSR

CSR（Control and Status Register）是特殊暫存器，只能用 `csrr`（讀）和 `csrw`（寫）存取。

| CSR | 誰寫入 | 內容 | 比喻 |
| --- | --- | --- | --- |
| `stvec` | 我們（kernel） | trap 發生時要跳去的位址 | 緊急聯絡處 |
| `sepc` | CPU 自動 | 出事那條指令的位址 | 書籤 |
| `scause` | CPU 自動 | trap 的原因編號 | 被叫走的理由 |
| `stval` | CPU 自動 | 額外資訊，例如出錯的位址 | 補充說明 |

注意：`sepc` 記的是 **`ebreak` 本身**的位址，不是下一條指令。直接回到 `sepc` 會再執行一次 `ebreak`（M2-2 會處理）。

`stvec` 的最低 2 bit 是模式（`00` = Direct mode，所有 trap 都跳到同一個位址），所以 trap 入口必須**對齊 4 bytes**。

## 5. 實驗觀察

### 開機時 `stvec` 已經有值

```text
stvec = 0000000080200000
```

`nm` 顯示 `0x80200000` 就是 `_start`。OpenSBI 跳進 kernel 之前，把 `stvec` 設成了 kernel 的進入點。

### 沒設定 `stvec` 就執行 `ebreak`

`RiftOS booted` 會一直重複印出。原因是 CPU 每次都跳到 `_start`，**從頭重跑整個 kernel**，從來沒有回到 `ebreak` 的下一行。

「離開」和「回來」是兩回事。目前只做到離開，回來要等到 M2-2。

## 6. 目前的 trap 流程

```text
ebreak ──→ stvec ──→ trap_entry（組語） ──→ trap_handler（C）──→ panic
                     kernel/trap_entry.S     kernel/trap.c
```

| 檔案 | 內容 |
| --- | --- |
| `include/trap.h` | 宣告 `trap_init()` |
| `kernel/trap.c` | `trap_init()`：把 `trap_entry` 寫進 `stvec`；`trap_handler()`：印出 `scause`、`sepc`、`stval`，然後 `panic` |
| `kernel/trap_entry.S` | `.align 2` 的 `trap_entry`：`call trap_handler`，後面接 `wfi` 迴圈當保險 |

為什麼要先經過組語的 `trap_entry`，不直接讓 `stvec` 指向 C 函式？

1. C 函式只保證對齊 2 bytes（因為開了壓縮指令），`stvec` 需要對齊 4。
2. trap 可能發生在任何一行程式中間。C 函式一開頭就會動到暫存器，M2-2 要先用組語把暫存器存起來。

## 7. 語法筆記

### inline asm

C 沒有讀寫 CSR 的語法，所以要在 C 裡面直接寫組語：

```c
// 讀：CSR → C 變數（輸出清單在第一個冒號後）
__asm__ volatile("csrr %0, stvec" : "=r"(x));

// 寫：C 值 → CSR（輸入清單在第二個冒號後）
__asm__ volatile("csrw stvec, %0" : : "r"(value));
```

- `%0`：由編譯器填入暫存器。`objdump` 看到的是 `csrr s0,stvec`，表示編譯器選了 `s0`。
- `r`：使用一般暫存器。`=` 表示這個運算元是輸出。

### 為什麼要 `volatile`

編譯器會把 asm 當成純計算：輸入相同，輸出就相同。實驗：連續讀兩次 `time` 再相減。

```text
沒有 volatile：li a0,0 / ret          ← 根本沒讀，直接回傳 0
有 volatile：  rdtime a5 / rdtime a0 / sub
```

### `.S` vs `.s`

- `.S`（大寫）：先經過 C 前處理器，可以用 `#include`、`#define`。
- `.s`（小寫）：直接交給組譯器。
- macOS 的檔案系統預設不分大小寫，Linux 會分。本專案統一使用 `.S`。

### `.align 2`

RISC-V 的 `.align n` 代表對齊 2ⁿ bytes，所以 `.align 2` 就是 4 bytes。

## 8. 工具

```sh
riscv64-elf-nm -n build/kernel.elf               # 列出符號和位址（-n 依位址排序）
riscv64-elf-nm -n build/kernel.elf | grep trap   # 只看跟 trap 有關的
riscv64-elf-objdump -d build/kernel.elf          # 反組譯，看編譯器實際產生的指令
```

位址會隨著程式碼增加而移動，每次比對都要重新跑 `nm`。

## 9. 踩過的坑

| 錯誤 | 症狀 | 怎麼發現的 |
| --- | --- | --- |
| `call trap_entry`（呼叫自己） | trap 時會無限循環、什麼都不印 | `objdump`：`jal 80200178 <trap_entry>` |
| 檔名寫成小寫 `.s` | macOS 上沒事，Linux 上找不到檔案 | `git status` 顯示 `trap_entry.s` |
| `uint64_t value;` 沒有初始化 | `stvec = 0` | `-Wuninitialized` warning、`objdump`：`li a5,0`、QEMU 輸出 |

教訓：**有 warning 不要略過**；用 `objdump` 確認編譯器實際做了什麼。
