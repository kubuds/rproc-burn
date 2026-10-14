# 固件格式约束

`burnd` 不解析固件，只是把文件交给 Linux 的 remoteproc 子系统，由 [sdk/](../sdk/) 补丁里的 `cvitek_remoteproc` 驱动加载。因此固件要满足的是**内核 5.10 通用 ELF 加载器**加这个驱动的要求，和 Arduino 无关。Arduino sketch、RT-Thread、裸机程序只要按下面链接都能烧。

## 必须满足

1. **ELF64，RISC-V，可执行。** `rproc_elf_sanity_check` 会检查 magic、`ET_EXEC`、机器类型。
2. **所有 `PT_LOAD` 段都落在小核的保留内存区内。** 驱动只把 DTS 里 `memory-region` 指向的 `rtos_dram` 映射给加载器，`p_paddr` 落在区外会报 `bad phdr` 并返回 `-EINVAL`。各板的区间：

   | 板 | SoC | 起始 | 大小 |
   | --- | --- | --- | --- |
   | Duo | CV1800B | `0x83F40000` | `0xC0000` (768 KiB) |
   | Duo256 | SG2002 | `0x8FE00000` | `0x200000` (2 MiB) |
   | DuoS / Duo Module 01 | SG2000 | `0x9FE00000` | `0x200000` (2 MiB) |

   这些值来自带补丁的 SDK 的 `memmap.py`（`CVIMMAP_FREERTOS_ADDR` / `_SIZE`），刷别的镜像要以镜像的 DTS 为准。
3. **必须有 `.resource_table` 段。** 5.10 的 `rproc_elf_load_rsc_table` 找不到它直接返回 `-EINVAL`，`START` 会失败并回 `OPTS_ERR`。内容可以是空表：

   ```c
   /* resource_table.c */
   #include <stdint.h>

   struct resource_table {
       uint32_t ver;
       uint32_t num;
       uint32_t reserved[2];
       /* uint32_t offset[num]; followed by entries, none here */
   };

   __attribute__((section(".resource_table"), used, aligned(8)))
   const struct resource_table resource_table = { .ver = 1, .num = 0 };
   ```

   链接脚本里把它放进某个会被加载的段：

   ```ld
   .resource_table : {
       KEEP(*(.resource_table))
   } > RAM
   ```
4. **入口必须在保留区起始地址。** 驱动的 `start()` 不写入口地址，只是清再置 `0x03003024` 的 bit 6 让 C906L 复位后从硬件复位向量跑，而复位向量就是 `rtos_dram` 的起点。所以链接脚本要把启动代码放在区间第一条指令处，ELF 的 `e_entry` 填多少都无所谓。
5. **文件别超过保留区大小。** 加载器只检查段，但 `burnd` 会把整个文件写进 `/lib/firmware`，根文件系统空间有限。

## 不需要的

- 不需要 vring / rpmsg：`num = 0` 的空资源表就够，驱动在没有 vdev 时不会建立通信通道。
- 不需要特定的段名或符号。`.text`、`.data` 怎么叫都行，只看 `PT_LOAD`。
- 不需要 BSS 预填：`PT_LOAD` 的 `p_memsz > p_filesz` 部分加载器会清零。

## 用 RT-Thread 举例

[kubuds/rt-thread](https://github.com/kubuds/rt-thread) 的 `bsp/cvitek/c906_little` 是小核 BSP。要让它走这条烧录链，需要：

1. 把 `link.lds` 的加载地址改到对应板的保留区起点，总大小控制在区间内。
2. 加上面的 `resource_table.c` 和链接脚本片段，确认 `readelf -S rtthread.elf` 能看到 `.resource_table`。
3. 确认启动代码（`start.S`）是区间内第一条指令，`readelf -l` 看第一个 `PT_LOAD` 的 `PhysAddr` 等于区间起点。
4. `burntool -p /dev/ttyACM0 -f rtthread.elf`。

小核的串口输出走哪个 UART 由 BSP 决定，和烧录无关；板子 Linux 侧的 `dmesg` 会打印 `remoteproc0: Started from 0x...`，可以确认加载是否成功。

## 排错

| 现象 | 可能原因 |
| --- | --- |
| `START` 回 `OPTS_ERR`，`dmesg` 有 `resource table not found` | 缺 `.resource_table` |
| `START` 回 `OPTS_ERR`，`dmesg` 有 `bad phdr` | 某个 `PT_LOAD` 不在保留区内 |
| 烧录成功、`remoteproc0: Started`，小核没反应 | 入口不在区间起点，或固件自身问题 |
| tail 之后等 20 s 回 `DATA_ERR` | 串口丢字节；降低波特率或换线，可加 `-v` 对比收发长度 |
| tail 之后等 20 s 回 `TIMEOUT` | tail 没被单独读到；增大 `--tail-delay` |
