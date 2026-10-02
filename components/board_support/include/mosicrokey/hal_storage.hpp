#pragma once
#include <esp_err.h>

namespace mosicrokey::hal
{
// 启动或存储工作上下文同步调用。重挂前 APP 排除游戏/Agent/文件访问。
// 使用 SDMMC 驱动超时；不自动格式化；仅挂载控制调用者拥有 card 句柄。
esp_err_t Hal_Storage_Mount();
// 状态读取/标记可跨任务。标记失败不立即卸载仍在访问的文件系统。
bool Hal_Storage_Is_Mounted();
void Hal_Storage_Mark_Failed();
// 启动时挂载固件内只读技能分区，不执行写入/格式化。
esp_err_t Hal_Storage_Mount_System();
} // namespace mosicrokey::hal
