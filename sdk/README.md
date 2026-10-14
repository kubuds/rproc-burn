## sdk

`0001-add-remoteproc-and-burn-support.patch` 是针对 Milk-V [duo-buildroot-sdk](https://github.com/milkv-duo/duo-buildroot-sdk) 的补丁，用来生成带烧录能力的 Arduino 版镜像。它做了几件事：

- 新增 `drivers/remoteproc/cvitek_remoteproc.c`（`cvitek,cv18xx-c906l-rproc`），用内核通用 ELF 加载器把固件放进保留内存，通过 `0x03003024` 的 bit 6 复位/释放 C906L。
- 在 `cv180x` / `cv181x` 的 memmap dtsi 里加 `rtos_dram` 保留区和 `c906l_rproc` 节点，默认固件名 `cvirtos.elf`。
- Duo / Duo256 / DuoS 三块板的 Linux defconfig 打开 `REMOTEPROC`、`RPMSG`，buildroot defconfig 打开 `ENABLE_FREERTOS` 以保留小核内存。
- 往三块板的 overlay 放入 `/usr/bin/burnd`，并在 `usb-rndis.sh` 里拉起它。

补丁里内嵌的 `burnd` 二进制是原始版本（libev 实现，路径写死），重新出镜像时用本仓库 Release 里的 `burnd-<ver>-riscv64` 替换即可，协议兼容。
