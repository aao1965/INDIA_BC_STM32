#include "fpga_pwm.h"

FSMC_Status_t fpga_pwm_init(uint16_t period_ticks, uint16_t duty_ticks)
{
    // 1. Safely stop PWM before changing parameters
    fpga_pwm_enable(0);

    // 2. Set new timing parameters
    fpga_pwm_set_period(period_ticks);
    fpga_pwm_set_duty(duty_ticks);

    // 3. Start PWM generation and return the final bus status
    return fpga_pwm_enable(1);
}

FSMC_Status_t fpga_pwm_set_period(uint16_t period_ticks)
{
    return fpga_write(ADDR_PWM_PERIOD, period_ticks);
}

FSMC_Status_t fpga_pwm_set_duty(uint16_t duty_ticks)
{
    // Hardware ignores duty_ticks > period_ticks,
    // but you can add a software clamp here if needed in the future.
    return fpga_write(ADDR_PWM_DUTY, duty_ticks);
}

FSMC_Status_t fpga_pwm_enable(uint8_t enable)
{
    // Using the awesome bitwise functions from fpga.h
    // This avoids the manual read-modify-write process in our upper logic
    if (enable) {
        return fpga_set_bit(ADDR_PWM_CTRL, FPGA_PWM_CTRL_ENABLE);
    } else {
        return fpga_clr_bit(ADDR_PWM_CTRL, FPGA_PWM_CTRL_ENABLE);
    }
}
