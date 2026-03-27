#include "led_blink.h"

/* --- НАСТРОЙКА ЯРКОСТИ --- */
#define MASTER_BRIGHTNESS 	7
#define L(val) ((uint8_t)((val * MASTER_BRIGHTNESS) / 100))

/* --- ПАЛИТРА --- */
static const LED_Color_t C_RED    = {L(255), 0,      0,      0};
static const LED_Color_t C_GREEN  = {0,      L(255), 0,      0};
//static const LED_Color_t C_BLUE   = {0,      0,      L(255), 0};
static const LED_Color_t C_YELLOW = {L(255), L(150), 0,      0};
static const LED_Color_t C_WHITEW = {0,      0,      0,      L(255)};
static const LED_Color_t C_OFF    = {0,      0,      0,      0};

/* --- ОПИСАНИЕ ФАЗ --- */
static const LED_Phase_t P_RG_OFF[] = {
    {C_RED, 500}, {C_GREEN, 500}, {C_OFF, 250}
};

static const LED_Phase_t P_BLINK_W[] = {
    {C_WHITEW, 500}, {C_OFF, 500}
};

static const LED_Phase_t P_TRAFFIC[] = {
    {C_RED, 1000}, {C_YELLOW, 400}, {C_GREEN, 1000}
};

static const LED_Phase_t P_SOS[] = {
    {C_RED, 150}, {C_OFF, 100}, {C_RED, 150}, {C_OFF, 100}, {C_RED, 150}, {C_OFF, 300},
    {C_RED, 450}, {C_OFF, 100}, {C_RED, 450}, {C_OFF, 100}, {C_RED, 450}, {C_OFF, 300},
    {C_RED, 150}, {C_OFF, 100}, {C_RED, 150}, {C_OFF, 100}, {C_RED, 150}, {C_OFF, 1000}
};

/* --- ТАБЛИЦА ЭФФЕКТОВ --- */
typedef enum { TYPE_DISCRETE, TYPE_TRANSITION } EffectType_t;

typedef struct {
    EffectType_t type;
    const LED_Phase_t *phases;
    uint16_t phase_count;
    LED_Color_t start_color;
    LED_Color_t end_color;
    uint32_t period_ms;
} EffectDesc_t;

static const EffectDesc_t EFFECT_TABLE[LED_STATE_TOTAL] = {
    [LED_STATE_OFF] = { TYPE_DISCRETE, NULL, 0 },
    [LED_STATE_RG_OFF] = { TYPE_DISCRETE, P_RG_OFF, 3 },
    [LED_STATE_BLINK_WHITE] = { TYPE_DISCRETE, P_BLINK_W, 2 },
    [LED_STATE_TRAFFIC_LIGHT] = { TYPE_DISCRETE, P_TRAFFIC, 3 },
    [LED_STATE_SOS] = { TYPE_DISCRETE, P_SOS, 18 },
    [LED_STATE_BREATH_WHITE] = { TYPE_TRANSITION, .start_color = {0,0,0,0}, .end_color = {0,0,0,L(255)}, .period_ms = 1500 },
    [LED_STATE_POLICE_FLASHER] = { TYPE_TRANSITION, .start_color = {L(255),0,0,0}, .end_color = {0,0,L(255),0}, .period_ms = 350 }
};

/* --- ПЕРЕМЕННЫЕ ДВИЖКА --- */
static LED_Handler_t *p_hled = NULL;
static LED_State_t current_state = LED_STATE_OFF;
static uint16_t current_phase_idx = 0;
static uint32_t timer_ms = 0;

/* --- РЕАЛИЗАЦИЯ ФУНКЦИЙ (Обязательно должны быть здесь!) --- */

void led_blink_init(LED_Handler_t *hled) {
    p_hled = hled;
    current_state = LED_STATE_OFF;
}

/**
 * @brief Возвращает текущее состояние (эффект)
 */
LED_State_t get_state_led(void) {
    return current_state;
}

void set_state_led(LED_State_t state) {
    if (!p_hled || state >= LED_STATE_TOTAL || current_state == state) return;

    current_state = state;
    timer_ms = 0;
    current_phase_idx = 0;

    RGB_LED_Stop(p_hled);

    const EffectDesc_t *ed = &EFFECT_TABLE[state];

    if (ed->type == TYPE_TRANSITION) {
        RGB_LED_StartTransition(p_hled, ed->start_color, ed->end_color, ed->period_ms, true);
    }
    else if (ed->type == TYPE_DISCRETE && ed->phases != NULL) {
        RGB_LED_SetColor(p_hled, ed->phases[0].color);
    }
}

void led_blink(void) {
    if (!p_hled || current_state == LED_STATE_OFF) return;

    const EffectDesc_t *ed = &EFFECT_TABLE[current_state];
    if (ed->type == TYPE_TRANSITION) return;

    timer_ms++;

    if (timer_ms >= ed->phases[current_phase_idx].duration_ms) {
        timer_ms = 0;
        current_phase_idx = (current_phase_idx + 1) % ed->phase_count;
        RGB_LED_SetColor(p_hled, ed->phases[current_phase_idx].color);
    }
}
