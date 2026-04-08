#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "fpga_ic.h"
#include "spi_fpga_bridge.h"

// Define the pin if not provided by board_support_package.h
#ifndef FPGA_INT_PIN
#define FPGA_INT_PIN 		GPIO_PIN_2
#endif

// Semaphore to wake up the task from EXTI interrupt
SemaphoreHandle_t xFpgaIrqSemaphore;

/* ========================================================================== */
/* Hardware Register Access Functions                                         */
/* ========================================================================== */

FSMC_Status_t fpga_ic_init_hw(void)
{
    FSMC_Status_t status;

    // 1. Ensure global interrupts are disabled initially (no stretcher config needed)
    status = fpga_write(ADDR_IC_CTRL, 0x0000);
    if (status != FSMC_OK) return status;

    // 2. Mask (disable) all individual interrupts
    status = fpga_write(ADDR_IC_MASK, 0x0000);
    if (status != FSMC_OK) return status;

    // 3. Set all edge detectors to Rising Edge (0) by default
    status = fpga_write(ADDR_IC_EDGE_SEL, 0x0000);
    if (status != FSMC_OK) return status;

    // 4. Clear any garbage pending flags by writing 1s (Write-1-to-Clear)
    status = fpga_write(ADDR_IC_PENDING, 0xFFFF);

    return status;
}

FSMC_Status_t fpga_ic_global_irq_enable(uint8_t enable)
{
    if (enable) {
        return fpga_set_bit(ADDR_IC_CTRL, FPGA_IC_CTRL_GLOBAL_EN);
    } else {
        return fpga_clr_bit(ADDR_IC_CTRL, FPGA_IC_CTRL_GLOBAL_EN);
    }
}

FSMC_Status_t fpga_ic_enable_irq(uint16_t irq_mask)
{
    return fpga_set_bit(ADDR_IC_MASK, irq_mask);
}

FSMC_Status_t fpga_ic_disable_irq(uint16_t irq_mask)
{
    return fpga_clr_bit(ADDR_IC_MASK, irq_mask);
}

FSMC_Status_t fpga_ic_get_pending(uint16_t *p_pending)
{
    // Passive read of pending flags
    return fpga_read(ADDR_IC_PENDING, p_pending);
}

FSMC_Status_t fpga_ic_clear_pending(uint16_t processed_flags)
{
    // Write back processed flags to clear them (Write-1-to-Clear)
    return fpga_write(ADDR_IC_PENDING, processed_flags);
}

/* ========================================================================== */
/* FreeRTOS Task and System Setup                                             */
/* ========================================================================== */

BaseType_t setup_fpga_interrupts(void)
{
    // 1. Create semaphore
    xFpgaIrqSemaphore = xSemaphoreCreateBinary();
    if (xFpgaIrqSemaphore == NULL) return pdFAIL;

    // 2. Init hardware (Interrupts stay globally disabled)
    if (fpga_ic_init_hw() != FSMC_OK) return pdFAIL;

    // 3. Unmask desired interrupt lines
    fpga_ic_enable_irq(FPGA_IRQ_TICK_1KHZ);

    // 4. Create Task
    BaseType_t task_status = xTaskCreate(vFpgaIrqTask, "FpgaIrq", 256, NULL, configMAX_PRIORITIES - 1, NULL);
    if (task_status != pdPASS) return pdFAIL;

    // 5. Final step: Enable Global Interrupts from FPGA to MCU
    fpga_ic_global_irq_enable(1);

    return pdPASS;
}

// Hardware EXTI Callback in STM32
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == FPGA_INT_PIN) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        // Clear EXTI pending bit to avoid infinite loops
        __HAL_GPIO_EXTI_CLEAR_IT(FPGA_INT_PIN);

        // Unblock the FPGA IRQ handler task
        xSemaphoreGiveFromISR(xFpgaIrqSemaphore, &xHigherPriorityTaskWoken);

        // Force context switch if a higher priority task was woken
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

uint32_t cnt_1kHz_interrupt = 0;

// FreeRTOS Task for processing FPGA events
void vFpgaIrqTask(void *pvParameters)
{
    uint16_t pending_flags;

    for(;;) {
        // Sleep until the FPGA pulls the interrupt line
        if (xSemaphoreTake(xFpgaIrqSemaphore, portMAX_DELAY) == pdTRUE) {

            // READ LOOP: Keep reading until all flags are cleared (W1C logic).
            // This prevents missing an interrupt if it fires right after we read the register.
            while (fpga_ic_get_pending(&pending_flags) == FSMC_OK && pending_flags != 0) {

                // 1. Process active flags
                if (pending_flags & FPGA_IRQ_TICK_1KHZ) {
                    cnt_1kHz_interrupt++;
                }

                // Place for future UART processing:
                // if (pending_flags & FPGA_IRQ_UART_EVENT) { ... }

                // 2. Explicitly clear only the flags we just processed
                fpga_ic_clear_pending(pending_flags);
            }
        }
    }
}
