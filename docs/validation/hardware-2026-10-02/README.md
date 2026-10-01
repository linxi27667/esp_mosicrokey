# 2026-10-02 v0.1.9 重连闭环验证

当前仅 USB 连接，COM8 / 同一 CP210x 适配器。板上固件 v0.1.9，应用 SHA256：`d5d21736728c7367eadec04d994e30f9c65d4f9babef3e61b0d5c520d885a869`。Git 仓库为 [linxi27667/esp_mosicrokey](https://github.com/linxi27667/esp_mosicrokey)，私有，main。

## 已实测

- 重连时 reset_reason=1、uptime=239972 ms，request=0/games=0；设备已经重新启动。不能据此确定上一轮中断当时的复位原因。
- Wi-Fi 配置保留并连接 Ll；普通 GLM 请求 1 成功，回复重连恢复成功。
- 原生 ESPClaw 请求 2 通过 game_preview result=0 发布已验证 V3/p1，completed status=0。源码来自先前板载 AI 修复版本，本轮通过真实工具重新发布。
- 从设备导出 2832 B，与参考源码逐字节一致，SHA256 54df1351ba60b757d340e16787566acfe7433aea7b4c83c687c612860b00b8d7；32 项同一受限 VM 玩法回放通过。
- 进入游戏 mode=4；实际 160×128 VM 画布 128 行/FNV 校验通过，923aa425，可见绿色蛇身和 SCORE 1。不是实体屏照片。

![重连后实际 VM 画布](evidence/v019-reconnect-frame.png)

## 长测状态

新的100次加载退出与独立两小时连续测试正在运行；当前不能标为通过。上一轮约100分钟因串口访问失败中断，保留为[历史部分结果](../hardware-2026-10-01/README.md)，不能合并时长。

精选脱敏文件及 SHA256 见 [evidence/record.json](evidence/record.json)。原始记录在 logs/hardware-2026-10-02/round-001。主机1178检查及13共享UI状态属于同一固件的先前软件基线，见[前轮证据](../hardware-2026-10-01/README.md)，本次没有把它们重新称为新的设备测试。

小智仍等待账号激活，真实语音、实体屏/按键、TF安装与重启持久化、完整语音并发尚未验收。USB预览重启丢失，电脑源码归档不是TF持久化。
