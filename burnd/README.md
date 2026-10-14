## burnd

运行在板子 Linux 侧的烧录守护进程。它监听 USB 串口 `/dev/ttyGS0`（115200），接收主机端 `burntool` 发来的固件，写到 `/lib/firmware/arduino.elf`，再通过 `/sys/class/remoteproc/remoteproc0` 停止、加载、启动小核（C906L）。协议见 [docs/protocol.md](../docs/protocol.md)。

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
