# sg200x-burn

给 SG200X 系列（Milk-V Duo / Duo256 / DuoS 等，CV1800B / SG2002 / SG2000）小核 C906L 烧写固件的一对工具：

| 目录 | 跑在哪 | 作用 |
| --- | --- | --- |
| [`burntool/`](burntool/) | 主机（Windows / Linux / macOS） | 通过 USB 串口把 ELF 发给板子，单文件 Go 二进制 |
| [`burnd/`](burnd/) | 板子的 Linux 侧 | 收固件、校验、经 remoteproc 重启小核，随 SD 卡镜像预装；串口、remoteproc 节点、固件名均可通过参数指定 |
| [`sdk/`](sdk/) | — | 让镜像支持 remoteproc 与 burnd 的 duo-buildroot-sdk 补丁 |
| [`docs/`](docs/) | — | 串口协议、固件格式约束 |

原本放在 [kubuds/sophgo-arduino](https://github.com/kubuds/sophgo-arduino) 的 `tools/` 下，拆出来单独维护，是因为烧录器本身和 Arduino 无关：任何按 [docs/firmware.md](docs/firmware.md) 链接的 ELF（Arduino sketch、RT-Thread、裸机程序）都能用它烧。

## 快速开始

1. 板子刷 Milk-V 的 Arduino 版镜像（自带 `burnd`），用 USB 线连到电脑，会多出一个串口（Linux 下 `/dev/ttyACM*`，Windows 下 `COMx`）。
2. 从 [Releases](../../releases) 下载对应平台的 `burntool`，解压。
3. 烧：

```bash
burntool -p /dev/ttyACM0 -f firmware.elf
```

成功后小核立即从新固件启动；固件会被保存在板子的 `/lib/firmware/arduino.elf`，上一版备份为 `arduino_old.elf`。

## 构建

```bash
# 主机端，需要 Go（版本见 burntool/go.mod）
cd burntool && go test ./... && go build -o burntool .

# 板端，需要 Debian/Ubuntu 的交叉编译器
sudo apt install gcc-riscv64-linux-gnu gcc-aarch64-linux-gnu
cd burnd && make                 # riscv64
cd burnd && make ARCH=aarch64    # aarch64
```

发布用的脚本在 `scripts/`：`build-burntool.sh` 交叉编译 8 个主机平台并打包，`build-burnd.sh` 出两个架构的板端二进制。打 `vX.Y.Z` 标签后 GitHub Actions 自动跑这两个脚本并发 Release。

## 发布产物

| 文件 | 说明 |
| --- | --- |
| `burntool-<ver>-<host>.tar.gz` / `.zip` | 主机端二进制，`<host>` 用 Arduino 的三元组命名（`x86_64-linux-gnu`、`arm64-apple-darwin`、`x86_64-mingw32` …），解压后只有一层 `burntool/` 目录 |
| `burntool-<ver>-systems.json` | 上面每个包的 URL、SHA-256、大小，Arduino Board Manager index 的 `systems` 字段格式，供 sophgo-arduino 的发布脚本直接引用 |
| `burnd-<ver>-riscv64` / `burnd-<ver>-aarch64` | 板端二进制，静态链接 |
| `SHA256SUMS` | 全部产物的校验和 |

## 与 sophgo-arduino 的关系

sophgo-arduino 的 Board Manager 包把 `burntool` 当作外部 tool 引用（和 xpack 的 gcc 一样），只记录这里 Release 的 URL 和校验和；两边版本号独立。串口协议两端都在本仓库，改协议时同时改两端并更新 `docs/protocol.md`。注意 `burnd` 一旦随镜像发出就很难更新，协议改动必须向后兼容。

## 许可

LGPL-2.1，沿用 sophgo-arduino。`burnd/lib` 下的 libev 与 zlib 为其各自许可。
