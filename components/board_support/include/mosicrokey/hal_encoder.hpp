#pragma once
#include <esp_err.h>

namespace mosicrokey::hal
{
// 单例 GPIO 双边沿采集。初始化/读取仅在任务上下文；读取原子取走累计格数。
// 四个合法相位边沿构成一格；反向抵消弹跳，非法双位跳变丢弃未完成格。
esp_err_t Hal_Encoder_Init();
esp_err_t Hal_Encoder_Read(int& detents);
} // namespace mosicrokey::hal
