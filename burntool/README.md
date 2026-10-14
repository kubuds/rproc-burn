## burntool

主机端烧录工具。通过板子的 USB 串口和 SD 卡镜像里的 `burnd` 守护进程通信（源码见 [../burnd](../burnd)），把 ELF 固件发到板上并重启小核。Arduino IDE 点击「上传」时由 `platform.txt` 调用它，也可以单独使用，烧任何满足 [docs/firmware.md](../docs/firmware.md) 约束的固件（例如 RT-Thread）。

纯 Go 实现，无 cgo，发布时交叉编译为单文件二进制，用户无需安装 Python 或其他依赖。

### 用法

```bash
burntool -p /dev/ttyACM0 -f firmware.elf      # Linux / macOS，USB 串口
burntool -p COM3 -f firmware.elf              # Windows
burntool -u http://192.168.42.1:8080/cgi-bin/burn -f firmware.elf   # HTTP，见 sdk/http
burntool -v -p /dev/ttyACM0 -f firmware.elf   # 打印每一帧，排查问题用
burntool --version
```

两种传输方式：串口走 `burnd` 的帧协议，HTTP 走 [sdk/http](../sdk/http) 里的 CGI（`PUT /firmware` 上传并重启）。`-p` 和 `-u` 二选一。

| 参数 | 说明 |
| --- | --- |
| `-p`, `--port` | 串口设备，也可用环境变量 `BURNTOOL_PORT` |
| `-u`, `--url` | HTTP 端点，也可用 `BURNTOOL_URL`；与 `-p` 互斥 |
| `-f`, `--firmware` | 要烧录的 ELF，也可用 `BURNTOOL_FIRMWARE` |
| `-b`, `--baud` | 波特率，默认 115200 |
| `--timeout` | 等待板端应答的秒数 |
| `--tail-delay` | 固件发完到发送校验帧之间的等待，默认 500 ms |
| `-v`, `--verbose` | 打印协议帧 |

退出码：0 成功，1 烧录失败，2 参数错误。

### 协议

8 字节控制帧，小端：

```
command : AA 55 | cmd u16 | length u32
reply   : AA 55 | cmd u16 | status u16 | errno u16
tail    : crc32 u32 | FF FF FF FF
```

上传流程：`NONE` → `CONNECT` → `STOP` → `UPDATE(size)` → 固件原始字节 → tail → 等待校验应答 → `START`。完整说明见 [docs/protocol.md](../docs/protocol.md)，实现见 `protocol.go` 和 `client.go`。

### 开发

```bash
go test ./...                       # 用内存管道模拟 burnd 跑协议测试
go build -o burntool .              # 本机构建
GOOS=windows GOARCH=arm64 go build  # 任意目标交叉编译
```

全部 host 的构建与打包由仓库根目录的 `scripts/build-burntool.sh` 完成，目标列表在脚本开头的 `HOSTS` 数组。
