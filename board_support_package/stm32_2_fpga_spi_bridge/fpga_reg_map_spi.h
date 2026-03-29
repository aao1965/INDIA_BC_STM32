/*
 * fpga_reg_map_spi.h
 * Кодировка: UTF-8. Карта регистров для SPI моста (Suffix S).
 * Соответствует логике Verilog: адрес 15 бит (7 бит ID модуля, 8 бит регистр).
 */

#ifndef FPGA_REG_MAP_SPI_H
#define FPGA_REG_MAP_SPI_H

#include <stdint.h>

/**
 * Константы архитектуры шины SPI
 * (Согласно дефайнам _D_S_... в Verilog)
 */
#define FPGA_S_ADDR_WIDTH        15    // Общая ширина адреса
#define FPGA_S_DEV_ADDR_WIDTH    7     // 7 бит на модули (биты 14..8)
#define FPGA_S_CHIP_ADDR_WIDTH   8     // 8 бит на регистры модуля (биты 7..0)

/**
 * Макрос для вычисления абсолютного адреса SPI
 * Пример: FPGA_S_ADDR(1, 0x02) -> 0x0102
 */
#define FPGA_S_ADDR(dev, reg)    ((((uint16_t)(dev) & 0x7F) << FPGA_S_CHIP_ADDR_WIDTH) | \
                                   ((uint16_t)(reg) & 0xFF))

/* ========================================================================== */
/*  МОДУЛЬ 1: DEBUG_MODULE (ID = 1)                                           */
/* ========================================================================== */
#define FPGA_S_DEV_DEBUG_ID      1

// Смещения регистров (Register Offsets)
#define REG_S_DEBUG_FEEDBACK_OFF 0x00  // Регистр эха (R/W)
#define REG_S_DEBUG_CONST_OFF    0x01  // Константа 0x3F5A (Read Only)
#define REG_S_DEBUG_MISC_OFF     0x02  // Misc/Control регистр (R/W)

// Абсолютные адреса для функций моста
#define ADDR_S_DEBUG_FEEDBACK    FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_FEEDBACK_OFF)
#define ADDR_S_DEBUG_CONST       FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_CONST_OFF)
#define ADDR_S_DEBUG_MISC        FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_MISC_OFF)

/* ========================================================================== */
/*  МОДУЛЬ 2: FCS_MODULE (ID = 2)                                             */
/* ========================================================================== */
#define FPGA_S_DEV_FCS_ID        2

#define REG_S_FCS_CONTROL_OFF    0x00
#define ADDR_S_FCS_CONTROL       FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_CONTROL_OFF)

#endif /* FPGA_REG_MAP_SPI_H */