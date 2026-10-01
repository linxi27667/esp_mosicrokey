#pragma once
#include "FreeRTOS.h"
BaseType_t xTaskCreate(void (*function)(void*),const char*,uint32_t,void*,unsigned,TaskHandle_t*);
inline BaseType_t xTaskCreatePinnedToCore(void (*function)(void*),const char* name,uint32_t stack,void* argument,unsigned priority,TaskHandle_t* handle,int) {
    return xTaskCreate(function,name,stack,argument,priority,handle);
}
TickType_t xTaskGetTickCount();
void vTaskDelay(TickType_t milliseconds);
void vTaskDelayUntil(TickType_t*,TickType_t);
inline unsigned uxTaskGetStackHighWaterMark(TaskHandle_t) {return 4096;}
