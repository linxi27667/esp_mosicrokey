#pragma once
#include <cstdint>

namespace mosicrokey::task_config {
// ESP-IDF 创建参数单位为字节。保持 v0.1.9 已验证的优先级与核分配。
constexpr uint32_t ui_stack_bytes = 8192, key_stack_bytes = 4096;
constexpr uint32_t app_stack_bytes = 8192, game_stack_bytes = 8192;
constexpr uint32_t console_stack_bytes = 8192, agent_stack_bytes = 16384;
constexpr uint32_t agent_receive_stack_bytes = 6144;
constexpr unsigned ui_priority = 3, key_priority = 3, app_priority = 2;
constexpr unsigned game_priority = 3, console_priority = 1, agent_priority = 1;
constexpr unsigned agent_receive_priority = 1;
constexpr int ui_core = 0, game_core = 1, agent_core = 0;
constexpr uint32_t key_period_ms = 5, game_period_ms = 33;
constexpr uint32_t app_wait_ms = 50, console_wait_ms = 100;
constexpr unsigned command_queue_capacity = 12;
}
