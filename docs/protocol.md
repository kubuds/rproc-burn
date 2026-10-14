# burnd 串口协议

主机端 `burntool` 和板端 `burnd` 之间走一条 115200 8N1 的 USB 串口（板端是 `/dev/ttyGS0`）。协议只有三种 8 字节帧，所有多字节整数小端。

## 帧格式

```
command : AA 55 | cmd u16 | length u32            主机 → 板
reply   : AA 55 | cmd u16 | status u16 | errno u16 板 → 主机
tail    : crc32 u32 | FF FF FF FF                  主机 → 板，结束固件传输
```

`length` 只对 `UPDATE` 有意义，是接下来要发送的固件字节数，其他命令填 0。`errno` 目前和 `status` 相同，可忽略。

### cmd

| 值 | 名称 | 板端动作 |
| --- | --- | --- |
| 0 | `NONE` | 无，仅回应，用来探测板子是否在线 |
| 1 | `CONNECT` | 无，仅回应 |
| 2 | `STOP` | 向 `remoteproc0/state` 写 `stop` |
| 3 | `START` | 向 `remoteproc0/firmware` 写 `arduino.elf`，再向 `state` 写 `start` |
| 4 | `UPDATE` | 进入接收状态，期待 `length` 字节固件 |
| 5 | `ERROR` | 不是请求；板端收到未知 cmd 时在 reply 里回这个值 |

### status

| 值 | 名称 | 含义 |
| --- | --- | --- |
| 0 | `OK` | 成功 |
| 1 | `TIMEOUT` | 板端任务超时（START 3 s，STOP 4 s，UPDATE 20 s） |
| 2 | `START_ERR` | 保留 |
| 3 | `STOP_ERR` | 保留 |
| 4 | `OPTS_ERR` | sysfs 打开/写入失败，或 `/lib/firmware` 不可写 |
| 5 | `SCHED_ERR` | 板端创建任务线程失败 |
| 6 | `CHECK_ERR` | 保留 |
| 7 | `DATA_ERR` | 收到的字节数或 CRC32 与声明不符 |

## 上传流程

```
主机                         板
 |-- NONE ------------------>|
 |<------------- reply OK ---|
 |-- CONNECT --------------->|
 |<------------- reply OK ---|
 |-- STOP ------------------>|      小核未运行时可能非 OK，主机忽略
 |<------------- reply -------|
 |-- UPDATE(len) ----------->|      板端清空缓冲，记录 len，开始计时
 |<------------- reply OK ---|
 |== 固件原始字节 len 个 ====>|      板端按 64 字节一读入队
 |   （停顿，默认 500 ms）    |
 |-- tail(crc32) ----------->|      板端落盘、算 CRC32、比对
 |<--- reply UPDATE status --|      最长 20 s
 |-- START ----------------->|
 |<------------- reply OK ---|
```

CRC32 是 zlib 的 `crc32()`（IEEE 802.3，和 Go 的 `crc32.ChecksumIEEE` 相同），对整个固件文件计算。

## 板端实现带来的约束

这些行为由 `burnd/src/data.c` 决定，写主机端实现时必须迁就：

- **按 64 字节读。** 板端每次 `read()` 最多拿 64 字节。接收期间所有数据原样入队，直到累计字节数等于 `length`。
- **tail 必须单独成一次读。** 累计字节数达到 `length` 后，下一次读到的 8 字节如果第 4–7 字节为 `FF FF FF FF`，才被当作 tail。若 tail 和最后一段固件数据落在同一次 `read()` 里，会被当作多余数据丢弃，板端永远等不到 tail，20 s 后超时。所以主机端在固件发完后要停顿再发 tail（`burntool` 默认 500 ms，`--tail-delay` 可调）。
- **接收期间不处理命令。** 从 `UPDATE` 应答到 tail 应答之间，`AA 55` 开头的数据都当固件内容；唯一例外是距 `UPDATE` 已超过 20 s，此时板端丢弃缓冲并退回空闲状态，之后的帧重新按命令解析。
- **固件路径固定。** 写到 `/lib/firmware/arduino.elf`，旧文件先改名为 `arduino_old.elf`。`START` 也固定加载 `arduino.elf`。
- **reply 可能夹杂杂音。** 串口刚打开时可能有残留字节，主机端应丢弃 `AA 55` 之前的内容再凑帧（`burntool` 的 `appendFrame` 就是干这个）。
- **STOP 的返回值不可靠。** `stop_task` 里的返回值是 `static`，小核本来就没在跑时会返回上一次的结果。主机端把 STOP 的非 OK 当作提示而不是错误。

## 兼容性

`burnd` 随 SD 卡镜像发布，用户很少更新，所以协议改动必须让旧 `burnd` 仍能工作：不要改已有 cmd 的语义，新增命令要容忍旧板端回 `ERROR`。
