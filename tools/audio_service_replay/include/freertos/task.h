#pragma once
#include "FreeRTOS.h"
typedef struct replay_task *TaskHandle_t;
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg,
                unsigned priority, TaskHandle_t *handle);
uint32_t ulTaskNotifyTake(int clear, uint32_t ticks);
void xTaskNotifyGive(TaskHandle_t task);
void vTaskDelay(uint32_t ticks);
