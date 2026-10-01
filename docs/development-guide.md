# 开发、构建与首次设备验证

## 本机环境

SDK：`E:\MCU\esp32\.espressif\v6.1\esp-idf`，ESP-IDF 6.1.0 / esp32s31。EIM 工具目录为 `...\v6.1\tools`，Python 虚拟环境为 `tools\python_env\v6.1\venv`。

`scripts/environment.ps1` 使用已核对的 EIM 路径；原 export.ps1 的额外 tools 层级不匹配本机安装。工程没有修改 SDK 安装。其他机器需调整环境脚本与主机工具路径。

```powershell
./scripts/build.ps1
./scripts/test-host.ps1
./scripts/package-release.ps1
```

构建先验证当前目录的 2960 个锁定源码文件，再离线反向应用补丁并核对六个修改文件的固定上游哈希。上游快照及必要补丁在当前工程；组件依赖由 dependencies.lock 锁定。首次下载依赖需要组件仓库或 SDK 缓存。Lua 为沿用上游版本的 5.5.0~7，LVGL 为 9.5.0。

`sdkconfig.defaults` 是默认配置，当前发布配置额外保存于 release/sdkconfig。全新配置必须保留这些关键设置：S31、16 MiB Flash、Octal PSRAM、中文、自定义板身份编译、Lua 栈 4096、LVGL canvas。UI 完整中文字体已生成并入工程，正常构建无需外部字体文件；可选再生成使用 scripts/generate_font.py 和指定 Noto Sans SC 文件。

RTOS tick 固定 1000 Hz，5 ms 按键/UI 延时须至少一个 tick；BSP 编译断言防止短延时取整为零。串口 status 输出全部任务的最低空闲栈字节数；App 定期输出内部堆/PSRAM 当前及最低余量，供设备验收调整栈和内存预算。

v0.1.4 起启动日志和 status 记录 SDK reset_reason，status 还记录 uptime_ms。Windows 长测期间仅为测试线程临时防止自动睡眠，结束时恢复；不更改系统电源计划。主机监视间隙超过 5 秒或设备运行时间倒退时验收中断，日志缺失区间不能算作通过。

## 烧录与日志

2026-10-01 已核对 COM8 为 Silicon Labs CP210x，芯片 ESP32-S31 rev0.0、Flash/PSRAM 均 16 MiB。后续重新插拔仍应核对端口和芯片。

分区烧录（正常开发，保留 NVS）：

```powershell
./scripts/build.ps1 -Flash -Port COM实际编号
. ./scripts/environment.ps1
idf.py -p COM实际编号 monitor
```

完整镜像烧录（偏移 0x0，会清除 NVS 配网和模型配置）：

```powershell
. ./scripts/environment.ps1
python -m esptool --chip esp32s31 -p COM实际编号 -b 460800 --no-stub write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x0 release/mosicrokey-v0.1.9-esp32s31.bin
```

首次检查 Flash/PSRAM 探测值、复位原因、ES8311 初始化、TF 挂载和 UI。所有硬件验证记录保存 COM、板卡修订、镜像 SHA256 和完整日志；日志分享前删去服务地址中可能的个人信息。

## 串口命令与首轮闭环

115200、8N1、UTF-8、LF；命令最长 6143 B，超长行丢弃。固件不回显输入、不保存历史；终端应关闭本地回显和命令历史，避免自行记录密钥。

| 命令 | 行为 |
|---|---|
| help / status | 命令列表 / Wi-Fi、TF、模式、请求及错误状态 |
| page home/voice/agent/games/settings/diag | 页面导航 |
| wifi | 进入小智配网；按屏幕提示接入热点并配置 |
| wifi_config JSON | 通过 ssid/password 保存配网并重启；不回显输入 |
| audio_status | 查看前台焦点、语音状态及 codec 输入/输出计数 |
| game_selftest | 真实 VM 样例性能、100 次加载释放、循环/内存/库限制诊断；需先退出游戏 |
| frame | 冻结并导出运行中 VM 画布；校验全部行及校验值后可生成 PNG |
| game_source ID [offset] | 只读导出当前实际源码分页；保留修订号，供电脑侧游戏回放测试 |
| agent_config JSON | 设置 url、model、key 至专用 NVS，忙碌时拒绝 |
| ask 文字 | 普通对话或候选分析，可离开页面继续后台执行 |
| gameask 创作需求 | 要求实际发布游戏；没有本轮发布记录则失败，拒绝模型虚报成功 |
| cancel | 请求取消；工具暂存任务在请求结束时清理 |
| demo | 从固件内的贪吃蛇源码校验并安装到 TF |
| remount | 手动重新挂载；Agent/游戏忙碌时拒绝 |
| key up/down/left/right/confirm/back 1或0 | 辅助注入按下/释放，用于设备输入回放 |
| reboot | 重启并再次枚举 TF 已提交版本 |

兼容服务配置例子：

```text
agent_config {"url":"https://你的服务地址/v1","model":"支持工具调用的模型名","key":"你的密钥"}
```

上游兼容后端对基础地址补全 `/chat/completions`，支持完整端点；使用 Bearer、tools、tool_choice=auto、max_tokens。实际服务必须支持这些字段。超时默认 60 秒，请求最长 4096 B。HTTPS 采用非阻塞轮询检查取消，取消完成前 busy 仍为真；HTTP 明文后端仍受其阻塞调用超时影响。

明确的 429/503 可在同一轮请求中退避 4/8 秒后重试，最多两次，共用该轮 60 秒预算；可随时取消。鉴权错误、不明确的传输失败和空模型输出不会自动重试。已发布的游戏不会因模型总结失败而被撤回，完成提示会保留发布事实。

v0.1.9 保留 SDK 建立连接的原始预算，连接成功后将 HTTPS 单次 I/O 等待限为 100 ms，断开后恢复连接预算；DNS/TCP 建立阶段仍由 SDK 管理。SDK 异步 POST 正文尚未完成时可能返回 WRITE_DATA；只有客户端处于已完成请求头、继续发送正文的状态才归一为 EAGAIN，使用保存的偏移继续同一请求。关闭状态、请求头和负值写错误不自动重放，原总期限不重置。

短 I/O 轮询可能产生 SDK 的 `Connection timed out before data was ready!` 提示；不能单凭这行认定整请求超时。查看同一 request 的最终 completed status/backend_status 和错误结果；原始日志保留，完整 60 秒期限与取消检查仍有效。

智谱免费服务基址 `https://open.bigmodel.cn/api/paas/v4`，当前实板创作使用 `glm-4-flash-250414`，前轮也测试过 `glm-4.7-flash`。不要将 `FlashX`、`GLM-4.7` 等付费型号当作免费型号。设备已覆盖真实请求、取消及 401/429 错误；不同版本的证据、实际断网/重连等剩余结论见实板记录。

建议首次顺序：配网 → 小智激活及真实语音 → demo → 贪吃蛇游玩/退出 → 配置 Agent → gameask 修改贪吃蛇 → 确认工具提交成功 → 重启 → 启动新版本。测试并发时保持游戏运行或小智对话，同时串口发起创作。

## 主机验证

tests/host 编译实际 App 状态机、GameService、Lua 5.5、LVGL 9.5 和同一页面源码；只替换 App 的 RTOS/网络/硬件接口。主机通过不代表真实 API、I2S/DMA/SDMMC 或设备时间预算通过。

```powershell
./scripts/test-host.ps1
# 使用装有 Pillow 的 Python 导出 PNG（原始 PPM 无需 Pillow）
python scripts/render-captures.py build-host/screenshots docs/validation/2026-10-01/screenshots
```

主机工具为本机 MinGW64、CMake 4.0.3 和 Ninja 1.12.1，路径集中在 test-host.ps1。所有测试数据仅生成在 build-host/test-card；脚本/测试不清理用户的 TF 卡。

## 目录

组件见[架构](architecture.md)，Lua 包见[游戏 API](game-api.md)。release 的完整镜像包含 bootloader、分区、初始 OTA 数据、app、system 与语音 assets；TF 上的 games/agent 是设备运行时创建的目录。




