#pragma once
#include <esp_err.h>
#include <cstdint>

namespace mosicrokey::hal
{
// 仅启动/UI 任务调用。初始化包含复位等待、1MHz SDA 寄存器探测及原绘制 IO 恢复。
// 无回读不阻止初始化；方向恢复或 IO 重建失败返回错误。成功后重复调用无副作用。
esp_err_t Hal_Display_Init();
// UI 独占；RGB565 内部 DMA 缓冲。完成信号等待 1 s，超时后排空驱动再返回。
// SDK 的参数提交/排空可能继续阻塞，1 s 不是整个函数的执行上限。
// 排空期间缓冲仍属于驱动；返回后已恢复字节序，调用者可继续使用。
// 坐标含两端；一次最多 160×20 像素，不接受空指针或越界区域。
esp_err_t Hal_Display_Flush(int x1, int y1, int x2, int y2, uint16_t* pixels);
} // namespace mosicrokey::hal
