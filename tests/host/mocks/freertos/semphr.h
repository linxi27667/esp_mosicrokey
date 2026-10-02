#pragma once
#include "FreeRTOS.h"
using SemaphoreHandle_t = void*;
SemaphoreHandle_t xSemaphoreCreateBinary();
void vSemaphoreDelete(SemaphoreHandle_t);
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t, BaseType_t*);
