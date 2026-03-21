/**
 * @file rgb_led.c
 * @brief Production-grade implementation with internal HAL Callbacks
 */

#include "rgb_led.h"
#include <string.h>

/**
 * @brief Linear interpolation using 64-bit intermediate math to prevent overflow
 */
static LED_Color_t LerpColor(LED_Color_t c1, LED_Color_t c2, uint32_t step, uint32_t total_steps) {
    LED_Color_t res;
    if (total_steps == 0) return c2;
    
    res.r = (uint8_t)((int64_t)c1.r + (int64_t)(c2.r - c1.r) * (int64_t)step / (int64_t)total_steps);
    res.g = (uint8_t)((int64_t)c1.g + (int64_t)(c2.g - c1.g) * (int64_t)step / (int64_t)total_steps);
    res.b = (uint8_t)((int64_t)c1.b + (int64_t)(c2.b - c1.b) * (int64_t)step / (int64_t)total_steps);
    res.w = (uint8_t)((int64_t)c1.w + (int64_t)(c2.w - c1.w) * (int64_t)step / (int64_t)total_steps);
    return res;
}

/**
 * @brief Physical update via DMA with race-condition protection for 'is_busy' flag
 * @note Protocol: G-R-B-W order, MSB first.
 */
static HAL_StatusTypeDef LowLevel_Update(LED_Handler_t *hled, LED_Color_t color) {
    HAL_StatusTypeDef status = HAL_ERROR;
    
    if (osMutexAcquire(hled->mutex, osWaitForever) == osOK) {
        uint32_t idx = 0;
        uint8_t data[4] = {color.g, color.r, color.b, color.w};
        
        for (int i = 0; i < 4; i++) {
            for (int bit = 7; bit >= 0; bit--) {
                hled->buffer[idx++] = (data[i] & (1 << bit)) ? RGB_LED_T1H : RGB_LED_T0H;
            }
        }
        
        while (idx < RGB_LED_BUFFER_SIZE) {
            hled->buffer[idx++] = 0;
        }

        hled->is_busy = true;
        status = HAL_TIM_PWM_Start_DMA(hled->htim, hled->channel, hled->buffer, RGB_LED_BUFFER_SIZE);
        
        if (status != HAL_OK) {
            hled->is_busy = false; /* Reset flag immediately if DMA failed to start */
        }
        
        osMutexRelease(hled->mutex);
    }
    return status;
}

/**
 * @brief Worker task: Orchestrates colors and animations
 */
static void RGB_LED_Task(void *argument) {
    LED_Handler_t *hled = (LED_Handler_t *)argument;
    RGB_LED_Cmd_t cmd;

    while (1) {
        if (osMessageQueueGet(hled->queue, &cmd, NULL, osWaitForever) == osOK) {
            
            if (cmd.type == RGB_LED_CMD_SET_COLOR) {
                LowLevel_Update(hled, cmd.start_color);
                osDelay(RGB_LED_LATCH_DELAY_MS);
            } 
            else if (cmd.type == RGB_LED_CMD_STOP) {
                LowLevel_Update(hled, (LED_Color_t){0,0,0,0});
                osDelay(RGB_LED_LATCH_DELAY_MS);
                hled->is_busy = false; 
            }
            else if (cmd.type == RGB_LED_CMD_START_TRANSITION) {
                bool interrupt_effect = false;
                
                while (!interrupt_effect) {
                    uint32_t total_steps = cmd.duration_ms / RGB_LED_STEP_MS;
                    if (total_steps == 0) total_steps = 1;

                    /* Forward Phase: Start -> End */
                    for (uint32_t i = 0; i <= total_steps; i++) {
                        if (osMessageQueueGetCount(hled->queue) > 0) {
                            interrupt_effect = true; break;
                        }
                        LowLevel_Update(hled, LerpColor(cmd.start_color, cmd.end_color, i, total_steps));
                        osDelay(RGB_LED_STEP_MS);
                    }

                    if (!cmd.is_cyclic || interrupt_effect) break;

                    /* Backward Phase: End -> Start */
                    for (uint32_t i = 0; i <= total_steps; i++) {
                        if (osMessageQueueGetCount(hled->queue) > 0) {
                            interrupt_effect = true; break;
                        }
                        LowLevel_Update(hled, LerpColor(cmd.end_color, cmd.start_color, i, total_steps));
                        osDelay(RGB_LED_STEP_MS);
                    }
                }
            }
        }
    }
}

/* --- API Implementation --- */

bool RGB_LED_Init(LED_Handler_t *hled, TIM_HandleTypeDef *htim, uint32_t channel) {
    if (hled == NULL || htim == NULL) return false;

    memset(hled, 0, sizeof(LED_Handler_t));
    hled->htim = htim;
    hled->channel = channel;

    hled->mutex = osMutexNew(NULL);
    hled->queue = osMessageQueueNew(RGB_LED_QUEUE_LEN, sizeof(RGB_LED_Cmd_t), NULL);
    
    const osThreadAttr_t attributes = {
        .name = "RGB_LED_Task",
        .stack_size = 512 * 4,
        .priority = osPriorityNormal
    };
    
    hled->thread = osThreadNew(RGB_LED_Task, hled, &attributes);
    return (hled->thread != NULL);
}

bool RGB_LED_SetColor(LED_Handler_t *hled, LED_Color_t color) {
    if (!hled || !hled->queue) return false;
    RGB_LED_Cmd_t cmd = {.type = RGB_LED_CMD_SET_COLOR, .start_color = color};
    return (osMessageQueuePut(hled->queue, &cmd, 0, 0) == osOK);
}

bool RGB_LED_StartTransition(LED_Handler_t *hled, LED_Color_t c1, LED_Color_t c2, uint32_t ms, bool cyclic) {
    if (!hled || !hled->queue) return false;
    RGB_LED_Cmd_t cmd = {
        .type = RGB_LED_CMD_START_TRANSITION,
        .start_color = c1,
        .end_color = c2,
        .duration_ms = ms,
        .is_cyclic = cyclic
    };
    return (osMessageQueuePut(hled->queue, &cmd, 0, 0) == osOK);
}

bool RGB_LED_Stop(LED_Handler_t *hled) {
    if (!hled || !hled->queue) return false;
    RGB_LED_Cmd_t cmd = {.type = RGB_LED_CMD_STOP};
    return (osMessageQueuePut(hled->queue, &cmd, 0, 0) == osOK);
}

bool RGB_LED_IsReady(LED_Handler_t *hled) {
    return (hled != NULL) ? !hled->is_busy : false;
}

/* --- HAL Callbacks (Integrated) --- */

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim != NULL && htim->Instance == TIM3) {
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0);
    }
}

void HAL_TIM_ErrorCallback(TIM_HandleTypeDef *htim) {
    if (htim != NULL && htim->Instance == TIM3) {
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0);
    }
}
