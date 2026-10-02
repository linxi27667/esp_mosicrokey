#pragma once
#include "FreeRTOS.h"
QueueHandle_t xQueueCreate(unsigned capacity,unsigned item_size);
BaseType_t xQueueSend(QueueHandle_t,const void*,TickType_t);
BaseType_t xQueueReceive(QueueHandle_t,void*,TickType_t);
void vQueueDelete(QueueHandle_t);
