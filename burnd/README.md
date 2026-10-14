## burnd

运行在板子 Linux 侧的烧录守护进程。它监听一个串口（默认 USB gadget 串口 `/dev/ttyGS0`，115200），接收主机端 `burntool` 发来的固件，写到固件目录，再通过 remoteproc 的 sysfs 接口停止、加载、启动小核。协议见 [docs/protocol.md](../docs/protocol.md)。

它只依赖内核标准的 remoteproc 接口，不含任何 SG200X 专属逻辑；换一块有 remoteproc 驱动的板子，调整下面的参数即可复用。

### 参数

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `-d`, `--device PATH` | `/dev/ttyGS0` | 监听的串口 |
| `-r`, `--rproc PATH\|NAME` | `/sys/class/remoteproc/remoteproc0` | 远端核的 sysfs 目录；只给名字时视为 `/sys/class/remoteproc/` 下的条目，如 `remoteproc1` |
| `-D`, `--firmware-dir DIR` | `/lib/firmware` | 固件存放目录。内核默认只从 `/lib/firmware` 加载，改它时要同步设置 `/sys/module/firmware_class/parameters/path` |
| `-f`, `--firmware NAME` | `arduino.elf` | 固件文件名，也是写进 remoteproc `firmware` 属性的名字；上一版备份为 `<name>_old.<ext>` |
| `-h`, `--help` | | 帮助 |

例如给第二个远端核烧 RT-Thread：

```bash
burnd -r remoteproc1 -f rtthread.elf
```

启动时若固件文件已存在，burnd 会先把它加载并启动（自动引导），再开始监听串口。

预编译的 `burnd` 随 Milk-V 的 Arduino 版 SD 卡镜像一起发布（见 [sdk/](../sdk/)），由镜像里的 `usb-rndis.sh` 在 USB 串口就绪后拉起。

### 构建

静态链接，依赖的 libev 与 zlib 已按架构放在 `lib/` 下，只需要交叉编译器。Debian/Ubuntu：

```bash
sudo apt install gcc-riscv64-linux-gnu gcc-aarch64-linux-gnu

make                   # riscv64：Duo / Duo256 / DuoS 的 Linux 侧
make ARCH=aarch64      # aarch64：DuoS 跑 ARM 大核时
```

也可以直接用仓库根目录的 `scripts/build-burnd.sh <version> [out-dir]` 两个架构一起出。

### 调试

默认不打日志。加 `CFLAGS+=-DDEBUG` 编译后用 syslog 输出，板上 `logread -f` 查看。
