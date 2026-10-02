#pragma once
#include "mosicrokey/contracts.hpp"

namespace mosicrokey
{
// 唯一启动组装；硬件→服务→任务，不另起 Wi-Fi/LVGL/audio 所有者。
Result App_System_Init();
// app_main 上下文调用一次，进入保留原优先级的小智主循环。
void App_System_Run();
} // namespace mosicrokey
