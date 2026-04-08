#ifndef FPGA_REG_MAP_H
#define FPGA_REG_MAP_H

#include <stdint.h>

/**
 * ============================================================================
 * FPGA Bus Architecture Constants
 * ============================================================================
 */
#define FPGA_CHIP_ADDR_WIDTH     6     // 6 bits for internal chip registers (0..63)
#define FPGA_DEV_ADDR_WIDTH      4     // 4 bits for module ID (0..15)

/**
 * Macro to calculate absolute address for STM32 FSMC read/write functions.
 * Example: FPGA_ADDR(1, 0) -> 64
 */
#define FPGA_ADDR(dev, reg)      (((dev) << FPGA_CHIP_ADDR_WIDTH) | (reg))


/**
 * ============================================================================
 * MODULE 1: DEBUG_MODULE (ID = 1)
 * ============================================================================
 */
#define FPGA_DEV_DEBUG_ID        1

// Register Offsets
#define REG_DEBUG_FEEDBACK_OFF   0x00  // Echo register (R/W)
#define REG_DEBUG_CONST_OFF      0x01  // Constant 0x3F5A (Read Only)
#define REG_DEBUG_MISC_OFF       0x02  // Misc register (R/W)

// Absolute Addresses
#define ADDR_DEBUG_FEEDBACK      FPGA_ADDR(FPGA_DEV_DEBUG_ID, REG_DEBUG_FEEDBACK_OFF)
#define ADDR_DEBUG_CONST         FPGA_ADDR(FPGA_DEV_DEBUG_ID, REG_DEBUG_CONST_OFF)
#define ADDR_DEBUG_MISC          FPGA_ADDR(FPGA_DEV_DEBUG_ID, REG_DEBUG_MISC_OFF)


/**
 * ============================================================================
 * MODULE 2: INTERRUPT_CONTROLLER (ID = 2)
 * ============================================================================
 */
#define FPGA_DEV_IC_ID           2

// Register Offsets (Updated for W1C architecture, RAW register removed)
#define REG_IC_PENDING_OFF       0x00  // R/W: Pending interrupts (Write-1-to-Clear)
#define REG_IC_MASK_OFF          0x01  // R/W: Mask (1 = Interrupt enabled)
#define REG_IC_EDGE_SEL_OFF      0x02  // R/W: 0 = Rising Edge, 1 = Falling Edge
#define REG_IC_CTRL_OFF          0x03  // R/W: Control Register

// Absolute Addresses
#define ADDR_IC_PENDING          FPGA_ADDR(FPGA_DEV_IC_ID, REG_IC_PENDING_OFF)
#define ADDR_IC_MASK             FPGA_ADDR(FPGA_DEV_IC_ID, REG_IC_MASK_OFF)
#define ADDR_IC_EDGE_SEL         FPGA_ADDR(FPGA_DEV_IC_ID, REG_IC_EDGE_SEL_OFF)
#define ADDR_IC_CTRL             FPGA_ADDR(FPGA_DEV_IC_ID, REG_IC_CTRL_OFF)

// --- Bitfields for CTRL Register ---
#define FPGA_IC_CTRL_GLOBAL_EN   (1 << 0)  // Bit 0: 1 = Global Interrupt Output Enabled

// Interrupt Vector Bitmasks
#define FPGA_IRQ_TICK_1KHZ       (1 << 0)  // Bit 0: 1 kHz System Tick


/**
 * ============================================================================
 * MODULE 3: PWM_CONTROLLER (ID = 3)
 * ============================================================================
 */
#define FPGA_DEV_PWM_ID          3

// Register Offsets
#define REG_PWM_CTRL_OFF         0x00  // R/W: Control register
#define REG_PWM_PERIOD_OFF       0x01  // R/W: Period in 10ns ticks
#define REG_PWM_DUTY_OFF         0x02  // R/W: Duty cycle in 10ns ticks

// Absolute Addresses
#define ADDR_PWM_CTRL            FPGA_ADDR(FPGA_DEV_PWM_ID, REG_PWM_CTRL_OFF)
#define ADDR_PWM_PERIOD          FPGA_ADDR(FPGA_DEV_PWM_ID, REG_PWM_PERIOD_OFF)
#define ADDR_PWM_DUTY            FPGA_ADDR(FPGA_DEV_PWM_ID, REG_PWM_DUTY_OFF)

// Bitmasks
#define FPGA_PWM_CTRL_ENABLE     (1 << 0)  // Bit 0: Set to 1 to enable PWM output

#endif /* FPGA_REG_MAP_H */
