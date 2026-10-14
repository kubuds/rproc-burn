## 通过 HTTP 烧录

`cgi-bin/burn` 是一个 busybox httpd 的 CGI 脚本，把 `burnd load` 暴露成三个 HTTP 接口，主机端用 `burntool -u` 或 `curl` 即可烧录，不走 USB 串口。Duo 系列镜像自带 RNDIS 网卡（板子 IP 192.168.42.1），直接可用；其他板子只要有网络和 busybox httpd 也一样。

| 方法 | 路径 | 作用 |
| --- | --- | --- |
| `POST` | `/cgi-bin/burn/firmware` | 请求体是固件文件；安装并重启小核 |
| `POST` | `/cgi-bin/burn/state` | 请求体 `start` 或 `stop` |
| `GET` | `/cgi-bin/burn/state` | 查询 remoteproc 状态 |

板上：

```sh
cp burn /www/cgi-bin/burn && chmod +x /www/cgi-bin/burn
httpd -p 8080 -h /www          # 或写进启动脚本
```

主机：

```sh
burntool -u http://192.168.42.1:8080/cgi-bin/burn -f firmware.elf
curl --data-binary @firmware.elf http://192.168.42.1:8080/cgi-bin/burn/firmware
curl -d stop http://192.168.42.1:8080/cgi-bin/burn/state
```

给 burnd 的参数（换 remoteproc 节点、固件名）通过环境变量 `BURND_OPTS` 传给脚本，例如在 httpd 的启动脚本里 `export BURND_OPTS="-r remoteproc1 -f rtthread.elf"`。

脚本直接输出完整的 HTTP 状态行，不依赖 httpd 对 `Status:` 头的处理，busybox 各版本通用。没有鉴权，只应在 RNDIS 这类点对点链路或可信局域网里开。
