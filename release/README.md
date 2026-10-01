# MosiCrokey v0.1.9 固件

目标：ESP32-S31-Function-CoreBoard-1 / esp32s31，ESP-IDF 6.1.0。

- `mosicrokey-v0.1.9-esp32s31.bin`：16 MiB 完整工厂镜像，烧录到 0x0；包括应用、语音/唤醒资源和只读游戏技能区。
- `mosicrokey-v0.1.9-app.bin`：应用镜像，当前偏移 0x20000。在分区布局和资源已匹配的设备上仅写此应用可保留 NVS；首次安装使用完整构建的分区与资源。偏移、长度和 SHA256 以 manifest.json 为准。
- `mosicrokey.elf`：对应调试符号，供崩溃分析。
- `manifest.json`、`SHA256SUMS.txt`：镜像、分段及源快照哈希。
- `sdkconfig`、`partitions.csv`、`dependencies.lock`：本次构建配置。
- `licenses/`：第三方许可副本。

**完整镜像会清除 NVS 配网与模型配置。** 正常开发分区烧录使用 `scripts/build.ps1 -Flash -Port 已确认的COM口`，保留 NVS。

已在 COM8 的 ESP32-S31 实板运行，真实免费智谱请求、原生游戏发布、取消/鉴权恢复及游戏版本隔离通过。1178 项主机检查通过。当前仅连接 USB；小智完整语音、实体显示/按键和 TF 持久化待验收，最新两小时运行结论以 `docs/validation/hardware-2026-10-02` 的记录为准；此前中断的测试保留在前日记录。接线和烧录步骤见 `docs/development-guide.md`。

生成的二进制、ELF 和构建配置保存在本地 release，不进入 Git 源码历史。镜像 SHA256 与分段信息见构建后生成的 manifest.json；发布标签及下载附件必须对应同一固件和验证记录。
