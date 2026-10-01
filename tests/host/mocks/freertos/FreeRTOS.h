#pragma once
#include <cstdint>
using TickType_t=uint32_t;
using BaseType_t=int;
using UBaseType_t=unsigned;
using TaskHandle_t=void*;
using QueueHandle_t=void*;
constexpr int pdPASS=1,pdTRUE=1,pdFALSE=0;
#define pdMS_TO_TICKS(ms) (ms)
struct MockStop {};
bool MockStopping();
void MockShutdown();
