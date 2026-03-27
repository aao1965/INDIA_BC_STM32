#ifndef LED_BLINK_H
#define LED_BLINK_H

#include "rgb_led.h"

/**
 * @brief Структура фазы эффекта
 */
typedef struct {
    LED_Color_t color;
    uint16_t duration_ms;
} LED_Phase_t;

/**
 * @brief Список состояний
 */
typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_RG_OFF,
    LED_STATE_BLINK_WHITE,
    LED_STATE_TRAFFIC_LIGHT,
    LED_STATE_SOS,
    LED_STATE_BREATH_WHITE,
    LED_STATE_POLICE_FLASHER,
    LED_STATE_TOTAL
} LED_State_t;

/* Прототипы функций (должны быть видимы для других файлов) */
void led_blink_init(LED_Handler_t *hled);
void set_state_led(LED_State_t state);
LED_State_t get_state_led(void);  // <-- Новая функция
void led_blink(void);

#endif /* LED_BLINK_H */
