#ifndef FPGA_PWM_H
#define FPGA_PWM_H

#include <stdint.h>
#include "fpga_reg_map.h"
#include "fpga.h"

/**
 * @brief Initializes the PWM module.
 * Safely stops PWM, sets the initial period and duty cycle, then enables the output.
 * @param period_ticks Total period in 10ns ticks (e.g., 10000 for 10 kHz).
 * @param duty_ticks   High-time in 10ns ticks.
 * @return FSMC_Status_t Status of the FSMC bus operations.
 */
FSMC_Status_t fpga_pwm_init(uint16_t period_ticks, uint16_t duty_ticks);

/**
 * @brief Sets the PWM period dynamically.
 * @param period_ticks Total period in 10ns ticks.
 * @return FSMC_Status_t Status of the FSMC write operation.
 */
FSMC_Status_t fpga_pwm_set_period(uint16_t period_ticks);

/**
 * @brief Sets the PWM duty cycle dynamically.
 * @param duty_ticks High-time in 10ns ticks.
 * @return FSMC_Status_t Status of the FSMC write operation.
 */
FSMC_Status_t fpga_pwm_set_duty(uint16_t duty_ticks);

/**
 * @brief Enables or disables the PWM hardware generation using hardware RMW.
 * @param enable 1 to start PWM, 0 to stop.
 * @return FSMC_Status_t Status of the FSMC bit operation.
 */
FSMC_Status_t fpga_pwm_enable(uint8_t enable);

#endif /* FPGA_PWM_H */
