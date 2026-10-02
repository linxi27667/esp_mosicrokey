#pragma once
#include <esp_err.h>
#include <cstddef>
#include <cstdint>

namespace mosicrokey::hal
{
struct key_event_t
{
    unsigned id;
    bool pressed;
};
// 启动时配置低有效输入与上拉；不启动任务或持有业务回调。
esp_err_t Hal_Key_Init();
// 仅 Key_Scan_Task 调用；不等待。id 顺序由 board_config::keys 定义。
esp_err_t Hal_Key_Read(unsigned id, bool& pressed);
// 20 ms 连续稳定去抖；now_ms 为单调毫秒，可自然溢出。
// 缓冲至少 keys.size() 项；事件复制给调用者，不存储外部指针。
esp_err_t Hal_Key_Scan(uint32_t now_ms, key_event_t* events, size_t capacity, size_t& count);
} // namespace mosicrokey::hal
