## burnd

运行在板子 Linux 侧的烧录守护进程。它监听一个串口（默认 USB gadget 串口 `/dev/ttyGS0`，115200），接收主机端 `burntool` 发来的固件，写到固件目录，再通过 remoteproc 的 sysfs 接口停止、加载、启动小核。协议见 [docs/protocol.md](../docs/protocol.md)。

它只依赖内核标准的 remoteproc 接口，不含任何 SG200X 专属逻辑；换一块有 remoteproc 驱动的板子，调整下面的参数即可复用。串口只是传输方式之一，见下面的 `load` 子命令。

### 参数

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `-d`, `--device PATH` | `/dev/ttyGS0` | 监听的串口 |
| `-r`, `--rproc PATH\|NAME` | `/sys/class/remoteproc/remoteproc0` | 远端核的 sysfs 目录；只给名字时视为 `/sys/class/remoteproc/` 下的条目，如 `remoteproc1` |
| `-D`, `--firmware-dir DIR` | `/lib/firmware` | 固件存放目录。内核默认只从 `/lib/firmware` 加载，改它时要同步设置 `/sys/module/firmware_class/parameters/path` |
| `-f`, `--firmware NAME` | `arduino.elf` | 固件文件名，也是写进 remoteproc `firmware` 属性的名字；上一版备份为 `<name>_old.<ext>` |
| `-v`, `--verbose` | | 记录每一帧和状态变化 |
| `-h`, `--help` | | 帮助 |

例如给第二个远端核烧 RT-Thread：

```bash
burnd -r remoteproc1 -f rtthread.elf
```

启动时若固件文件已存在，burnd 会先把它加载并启动（自动引导），再开始监听串口。

### 一次性模式：`burnd load FILE`

不开串口，直接停小核、把 `FILE` 拷到固件目录（旧文件备份）、重新启动，然后退出。退出码 0 成功、1 失败，原因打印到 stderr。它是「任意方式上传 + 触发」的通用入口：

```sh
scp firmware.elf root@192.168.42.1:/tmp/ && ssh root@192.168.42.1 burnd load /tmp/firmware.elf
burnd load /lib/firmware/arduino.elf        # 文件已在原位时只做重启
```

[sdk/http](../sdk/http) 用它实现了一个 HTTP 接口。

### 结构

```
src/main.c      参数、自动引导、进入服务循环或 load 模式
src/service.c   串口服务：poll 循环 + 状态机（空闲 → 接收 → 校验尾帧）
src/loader.c    remoteproc 操作：stop / start / 落盘 / 备份，与传输方式无关
src/serial.c    打开 tty，raw 8N1 115200
src/config.c    命令行参数与派生路径
src/crc32.c     IEEE CRC-32，与 zlib 的 crc32() 相同
src/log.c       stderr + syslog
```

单线程，没有第三方依赖，只用 libc。接收期间固件先写到 `<name>.tmp`，CRC32 校验通过后才备份旧版并原子改名，传一半掉电不会留下损坏的固件。日志默认只记错误，走 syslog（`logread`）和 stderr；`-v` 会记录每一帧。

### 构建

只需要交叉编译器。Debian/Ubuntu：

```bash
sudo apt install gcc-riscv64-linux-gnu libc6-dev-riscv64-cross gcc-aarch64-linux-gnu libc6-dev-arm64-cross

make                   # riscv64：Duo / Duo256 / DuoS 的 Linux 侧
make ARCH=aarch64      # aarch64：DuoS 跑 ARM 大核时
make ARCH=native       # 本机，跑测试用
```

静态链接，与镜像的 libc 版本无关。仓库根目录的 `scripts/build-burnd.sh <version> [out-dir]` 两个架构一起出。

### 测试

```bash
make ARCH=native test     # 本机构建
make ARCH=riscv64 test    # 交叉构建的二进制在 qemu-riscv64-static 下跑同一套测试
```

`tests/run.sh` 用 socat 建一对 pty、一个模拟内核行为的假 sysfs 目录，再用 `tests/client.py`（纯 Python，不依赖 pyserial）按协议跑：正常上传、尾帧紧贴数据、二次上传留备份、CRC 错误被拒且旧固件不受影响、STOP/START、未知命令、帧前杂音。需要 `socat` 和 `python3`；跑交叉构建还需要 `qemu-user-static`（脚本通过 `BURND_RUNNER` 环境变量接收运行器）。
