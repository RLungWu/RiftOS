# M2-2 學習筆記：讓 trap 回來

> 進度：M2-2 完成。trap 可以離開正常程式碼，並且原封不動地回來。

## 1. 回來靠 `sret`

`sret`（supervisor return）做兩件事：

- 跳到 **`sepc`** 存的位址，也就是書籤的位置。
- 恢復 trap 前的權限等級和中斷狀態。CPU 進 trap 時會自動把這些記在 `sstatus` 裡。

## 2. 書籤要往後挪

`sepc` 指的是 `ebreak` **本身**。直接 `sret` 會再執行一次 `ebreak`，變成無限循環。所以回去之前要先挪動書籤：

```c
if (scause == 3) {          // breakpoint
    sepc += 2;              // c.ebreak 只有 2 bytes（objdump：9002）
    __asm__ volatile("csrw sepc, %0" : : "r"(sepc));
    return;
}
panic("unexpected trap");   // 其他例外還不會處理
```

```text
80200026:  9002       ebreak        ← sepc
80200028:  00000517   auipc a0,0x0  ← sepc + 2，sret 跳回這裡
```

限制：`+2` 是寫死的，只適用於 `c.ebreak`。

## 3. 只有 `sret` 還不夠：暫存器會被改掉

只挪書籤、不保存暫存器時，畫面上也會印出 `Return from trap`，看起來好像已經成功了。但這只是**運氣好**。

### 用會失敗的測試把問題抓出來

```c
__asm__ volatile(
    "li a0, 0x1234\n"
    "ebreak\n"
    "mv %0, a0"
    : "=r"(v)
    :
    : "a0"          // clobber：告訴編譯器這段會改 a0
);
```

結果：

```text
a0 after trap = 00000000802001EA     ← 不是 0x1234
```

`0x802001EA` 是 `"\r\n"` 字串的結尾。`trap_handler` 最後呼叫 `uart_puts("\r\n")`，`a0` 一路被 `message++` 推到 `\0` 的位置。

教訓：**先寫一個會失敗的測試，再修 bug**。修完之後，才有證據證明真的修好了。

### 為什麼這很重要

M2-4 的時鐘中斷**隨時都可能發生**，可能在任何一條指令中間打斷程式。如果回來後暫存器被改掉，程式會在完全看不出原因的地方壞掉。

## 4. trap frame

像出門前先拍照，回家後照著照片把房間恢復原狀：

```text
trap_entry:
    addi sp, sp, -256        ← 在 stack 上挖 256 bytes
    sd   x1,  8(sp)          ← 拍照：存 30 個暫存器
    ...
    call trap_handler        ← C 隨便改暫存器都沒關係
    ld   x1,  8(sp)          ← 照著照片恢復
    ...
    addi sp, sp, 256         ← 把空間還回去
    sret
```

| 暫存器 | 存不存 | 原因 |
| --- | --- | --- |
| `x0` | 不存 | 永遠是 0 |
| `x2`（`sp`） | 不另外存 | 正在用它指向 trap frame，`addi sp, sp, 256` 就恢復了 |
| 其他 30 個 | 全存 | 最簡單、最安全 |

配置規則：**`xN` 放在 offset `N × 8`**。32 × 8 = 256 bytes，剛好是 16 的倍數（RISC-V 規定 `sp` 要 16 對齊）。

修好之後：

```text
a0 after trap = 0000000000001234     ← 完整恢復
```

## 5. 為什麼 trap 入口一定要用組語寫

`call trap_handler` 其實是 `jal ra, trap_handler`，它會把返回位址寫進 **`x1`（`ra`）**。

- 舊寫法：一進來就 `call`，被打斷的程式放在 `ra` 裡的值直接消失。
- 新寫法：先 `sd x1, 8(sp)` 存起來，`call` 蓋掉也沒關係，回來再 `ld x1, 8(sp)` 恢復。

存檔必須發生在**任何**暫存器被改動之前。C 函式做不到，因為它的開頭至少會動到 `sp`。

## 6. 語法筆記

### `sd` / `ld` / `addi`

```asm
addi sp, sp, -256   # sp = sp - 256（stack 往低位址長）
sd   x5, 40(sp)     # store doubleword：記憶體[sp + 40] = x5
ld   x5, 40(sp)     # load doubleword：x5 = 記憶體[sp + 40]
```

建議寫 `x5` 而不是別名 `t0`，這樣編號和 offset 一眼就對得起來。

### inline asm 的三個冒號

```c
__asm__ volatile("指令" : 輸出 : 輸入 : clobber);
```

- 輸出：`"=r"(v)`
- 輸入：`"r"(value)`
- clobber：`"a0"`，告訴編譯器「這段會改 `a0`，不要在裡面放你需要的東西」

把 `li`、`ebreak`、`mv` 放在同一個 `asm` 裡，是為了保證它們連在一起執行，中間不會被編譯器插入其他指令。
