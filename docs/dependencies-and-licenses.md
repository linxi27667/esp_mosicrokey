# 依赖来源、版本与许可记录

版本以 upstream-lock.json、dependencies.lock、source-manifest.json 和 release/manifest.json 为准。构建源码与补丁均在当前项目，首次组件下载使用官方组件仓库或本机缓存。

## 上游快照

| 项目 | 提交 / 许可 | 集成方式 |
|---|---|---|
| [小智](https://github.com/78/xiaozhi-esp32) | ed5cd7ba0011dc06fefd0762790857268c41b430 / MIT | third_party/xiaozhi，1714 个文件；产品补丁记录在 patches |
| [ESPClaw](https://github.com/espressif/esp-claw) | 74b18700a1d6c40de472bfc71c19e49356ca1cc0 / Apache-2.0 | third_party/esp-claw，1246 个文件；公开接口适配与已记录的 HTTP/工具兼容小补丁 |
| ESP-IDF | 6.1.0，esp32s31 / SDK 所附 Apache-2.0 等许可 | 本机 SDK，不复制进工程 |

保留各快照 LICENSE、NOTICE 和源文件声明。ESPClaw 编入 claw_core、cap、context、router、memory、skill、paths 等所需模块；小智排除原始入口、板工厂及其他板实现。

## 托管组件锁定结果

| 组件 | 精确版本 |
|---|---|
| lvgl/lvgl | 9.5.0 |
| georgik/lua | 5.5.0~7（Lua 5.5.0） |
| 78/xiaozhi-fonts | 2.0.0 |
| 78/esp-wifi-connect | 3.3.1 |
| 78/esp-ml307 | 3.7.5 |
| 78/uart-uhci | 0.4.0 |
| espressif/button | 4.2.1 |
| espressif/cjson | 1.7.19~2 |
| espressif/esp-sr | 2.4.7 |
| espressif/esp_audio_codec | 2.5.0 |
| espressif/esp_audio_effects | 1.3.0~1 |
| espressif/esp_codec_dev | 1.6.2 |
| espressif/esp_new_jpeg | 1.0.2 |
| espressif/led_strip | 3.0.3 |
| espressif/mqtt | 1.1.0 |
| espressif/esp-dsp | 1.8.0 |
| espressif/dl_fft | 0.8.0 |
| espressif/gmf_fft | 1.0.0 |
| espressif/cmake_utilities | 1.1.1 |

78/esp-ml307 同时提供小智使用的 esp_network Wi-Fi HTTP/MQTT 接口，本产品未声明使用蜂窝硬件。esp_new_jpeg 是保留的小智依赖，产品没有编入上游 JPEG 编解码源文件。实际链接结果以 ELF/map 为准。

LVGL 和 Lua 核心所附 MIT 声明分别保存在组件许可文件与 lua.h 末尾；LVGL 内含其他第三方子库的许可。语音组件包含 Espressif 专用/修改的 MIT 条款，不能统一标为普通 MIT：例如 esp-sr 的许可声明使用 Espressif 产品，esp_audio_codec 的许可声明仅与 Espressif 产品使用。详细条文保留原件，当前硬件基线为 Espressif S31。

## 字体与发布材料

项目 UI 字体由 Noto Sans SC 14 px 栅格化生成，覆盖 ASCII、基本汉字与常用标点，共 21503 个字形，1 bpp。生成后的 C 文件已纳入 assets/fonts，正常构建不依赖原字体位置；生成脚本使用 Pillow。字体采用 SIL OFL 1.1，原许可位于 assets/fonts/OFL.txt，来源与文件哈希见 assets/fonts/README.md。

packaging 脚本将上游、SDK、托管组件和字体的许可副本附到 release/licenses。首版自有码的公开发布许可证尚未由用户选择，当前交付不自动替项目另选许可证。商用发布前保留相关声明并核对所用服务、语音资源与组件的条件。

## 可复查资料

- source-manifest.json：当前已应用补丁的全部上游文件 SHA256。
- patches/xiaozhi-mosicrokey.patch：相对固定提交的最小产品改动。
- patches/espclaw-glm-auth.patch：HTTPS 取消、401 原因、tool_choice 及 429/503 有界退避；基线提交与理由见 patches/README.md。
- dependencies.lock：托管组件版本及仓库哈希。
- release/manifest.json、SHA256SUMS.txt、sdkconfig：本次镜像、源码快照、分段及配置记录。
