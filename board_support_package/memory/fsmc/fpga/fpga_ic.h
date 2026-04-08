#ifndef FPGA_IC_H
#define FPGA_IC_H

#include <stdint.h>
#include "fpga.h"
#include "fpga_reg_map.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/**
 * @brief Initializes the FPGA Interrupt Controller hardware.
 * Configures basic registers, masks all interrupts, clears pending flags via W1C,
 * and KEEPS GLOBAL INTERRUPT DISABLED.
 * @return FSMC_Status_t Status of the FSMC bus operations.
 */
FSMC_Status_t fpga_ic_init_hw(void);

/**
 * @brief Sets or clears the Global Interrupt Enable bit.
 * @param enable 1 to enable global interrupts to STM32, 0 to disable.
 * @return FSMC_Status_t Status of the FSMC write operation.
 */
FSMC_Status_t fpga_ic_global_irq_enable(uint8_t enable);

/**
 * @brief Enables specific interrupts inside the FPGA.
 * @param irq_mask Bitmask of interrupts to enable (e.g. FPGA_IRQ_TICK_1KHZ).
 * @return FSMC_Status_t Status of the FSMC write operation.
 */
FSMC_Status_t fpga_ic_enable_irq(uint16_t irq_mask);

/**
 * @brief Disables specific interrupts inside the FPGA.
 * @param irq_mask Bitmask of interrupts to disable.
 * @return FSMC_Status_t Status of the FSMC write operation.
 */
FSMC_Status_t fpga_ic_disable_irq(uint16_t irq_mask);

/**
 * @brief Reads pending interrupts. Hardware DOES NOT clear them automatically anymore!
 * @param p_pending Pointer to store the pending interrupt bitmask.
 * @return FSMC_Status_t Status of the FSMC read operation.
 */
FSMC_Status_t fpga_ic_get_pending(uint16_t *p_pending);

/**
 * @brief Clears pending interrupts using Write-1-to-Clear logic.
 * @param processed_flags Bitmask of the interrupts that have been processed and should be cleared.
 * @return FSMC_Status_t Status of the FSMC write operation.
 */
FSMC_Status_t fpga_ic_clear_pending(uint16_t processed_flags);

/* ========================================================================== */
/* FreeRTOS Integration                                                       */
/* ========================================================================== */

/**
 * @brief Initializes the OS objects and configures the FPGA for interrupts.
 */
BaseType_t setup_fpga_interrupts(void);

/**
 * @brief FreeRTOS task that handles events originating from the FPGA.
 * @param pvParameters Pointer to task parameters.
 */
void vFpgaIrqTask(void *pvParameters);

#endif /* FPGA_IC_H */
