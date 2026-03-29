#ifndef FPGA_REG_MAP_H
#define FPGA_REG_MAP_H

/**
 * FPGA Bus Architecture Constants
 * (Matching define.v and main.v logic)
 */
#define FPGA_CHIP_ADDR_WIDTH     6     // 6 bits for registers (0..63)
#define FPGA_DEV_ADDR_WIDTH      4     // 4 bits for modules (0..15)

/**
 * Macro to calculate absolute address for fpga_read/write functions
 * Example: FPGA_ADDR(1, 0) -> 64
 */
#define FPGA_ADDR(dev, reg)      (((dev) << FPGA_CHIP_ADDR_WIDTH) | (reg))

/* ========================================================================== */
/*  MODULE 1: DEBUG_MODULE (ID = 1)                                           */
/* ========================================================================== */
#define FPGA_DEV_DEBUG_ID        1

// Register Offsets (P_ADD_... in Verilog)
#define REG_DEBUG_FEEDBACK_OFF   0x00  // Echo register (R/W)
#define REG_DEBUG_CONST_OFF      0x01  // Constant 0x3F5A (Read Only)
#define REG_DEBUG_MISC_OFF       0x02  // Misc register (R/W, default 0x0500)

// Absolute Addresses for STM32 functions
#define ADDR_DEBUG_FEEDBACK      FPGA_ADDR(FPGA_DEV_DEBUG_ID, REG_DEBUG_FEEDBACK_OFF)
#define ADDR_DEBUG_CONST         FPGA_ADDR(FPGA_DEV_DEBUG_ID, REG_DEBUG_CONST_OFF)
#define ADDR_DEBUG_MISC          FPGA_ADDR(FPGA_DEV_DEBUG_ID, REG_DEBUG_MISC_OFF)

/* ========================================================================== */
/*  MODULE 2: FCS_MODULE (ID = 2) - Example Template                          */
/* ========================================================================== */
#define FPGA_DEV_FCS_ID          2

#define REG_FCS_CONTROL_OFF      0x00
#define ADDR_FCS_CONTROL         FPGA_ADDR(FPGA_DEV_FCS_ID, REG_FCS_CONTROL_OFF)

#endif /* FPGA_REG_MAP_H */