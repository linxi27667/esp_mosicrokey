#pragma once
#include "mosicrokey/app.hpp"

namespace mosicrokey
{
// Console_Task 独占，命令无回显/历史；6144 B 行缓冲由任务拥有。
// 模型请求异步提交，诊断/文件用例在低优先级串口任务执行。
void Console_Command_Process(App& app, const std::string& line);
Result Console_Task_Create(App& app);
Result Key_Scan_Task_Create(App& app);
} // namespace mosicrokey
