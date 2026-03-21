/**
 * @file rgb_led.h
 * @brief Thread-safe RGBW LED driver for IN-PI55 (STM32F4, 84MHz APB1)
 * @author Gemini
 * @date 2026
 * @encoding UTF-8
 */

#ifndef RGB_LED_H
#define RGB_LED_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdbool.h>

/* --- Hardware Timing Constants (84 MHz, ARR = 104) --- */
#define RGB_LED_T1H              74    /* Logical 1: ~0.88us High */
#define RGB_LED_T0H              30    /* Logical 0: ~0.35us High */
#define RGB_LED_RESET_PULSES     60    /* Reset signal: >75us Low pulses */

/* --- Buffer & Queue Configuration --- */
#define RGB_LED_BUFFER_SIZE      (32 + RGB_LED_RESET_PULSES)
#define RGB_LED_QUEUE_LEN        10

/* --- Animation & Latch Timing --- */
#define RGB_LED_STEP_MS          20    /* 50 Hz animation update rate */
#define RGB_LED_LATCH_DELAY_MS   5     /* Time for LED to latch data (Reset period) */

/**
 * @brief RGBW Color structure
 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
} LED_Color_t;

/**
 * @brief Command types for the LED Task
 */
typedef enum {
    RGB_LED_CMD_SET_COLOR,
    RGB_LED_CMD_START_TRANSITION,
    RGB_LED_CMD_STOP
} RGB_LED_Cmd_Type_t;

/**
 * @brief Command structure for OS Message Queue
 */
typedef struct {
    RGB_LED_Cmd_Type_t type;
    LED_Color_t start_color;
    LED_Color_t end_color;
    uint32_t duration_ms;
    bool is_cyclic;
} RGB_LED_Cmd_t;

/**
 * @brief LED Handle structure
 */
typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    uint32_t buffer[RGB_LED_BUFFER_SIZE];
    
    osMessageQueueId_t queue;
    osThreadId_t thread;
    osMutexId_t mutex;
    
    volatile bool is_busy;
} LED_Handler_t;

/* --- Public API --- */
bool RGB_LED_Init(LED_Handler_t *hled, TIM_HandleTypeDef *htim, uint32_t channel);
bool RGB_LED_SetColor(LED_Handler_t *hled, LED_Color_t color);
bool RGB_LED_StartTransition(LED_Handler_t *hled, LED_Color_t c1, LED_Color_t c2, uint32_t ms, bool cyclic);
bool RGB_LED_Stop(LED_Handler_t *hled);
bool RGB_LED_IsReady(LED_Handler_t *hled);

#endif /* RGB_LED_H */