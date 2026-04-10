/*
 * fpga_reg_map_spi.h
 * Кодировка: UTF-8.
 * Карта регистров для SPI моста.
 * Адрес 10 бит: [9:6] - ID модуля (4 бита), [5:0] - адрес регистра (6 бит).
 */

#ifndef FPGA_REG_MAP_SPI_H
#define FPGA_REG_MAP_SPI_H

#include <stdint.h>

/**
 * Константы архитектуры шины
 */
#define FPGA_S_ADDR_WIDTH        10    // Общая ширина адреса
#define FPGA_S_DEV_ADDR_WIDTH    4     // Бит на ID модуля (биты 9..6)
#define FPGA_S_CHIP_ADDR_WIDTH   6     // Бит на регистры внутри модуля (биты 5..0)

/**
 * Макрос для вычисления абсолютного адреса
 * Пример: FPGA_S_ADDR(1, 0x02) -> 0x0042
 */
#define FPGA_S_ADDR(dev, reg)    ((((uint16_t)(dev) & 0x0F) << FPGA_S_CHIP_ADDR_WIDTH) | \
                                   ((uint16_t)(reg) & 0x3F))

/**
 * МОДУЛЬ 1: DEBUG_MODULE (ID = 1)
 */
#define FPGA_S_DEV_DEBUG_ID      1

#define REG_S_DEBUG_FEEDBACK_OFF 0x00  // Регистр эха (R/W)
#define REG_S_DEBUG_CONST_OFF    0x01  // Константа для проверки связи (0x3F5A)
#define REG_S_DEBUG_MISC_OFF     0x02  // Разное/Управление

#define ADDR_S_DEBUG_FEEDBACK    FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_FEEDBACK_OFF)
#define ADDR_S_DEBUG_CONST       FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_CONST_OFF)
#define ADDR_S_DEBUG_MISC        FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_MISC_OFF)

/**
 * МОДУЛЬ 2: FCS_MODULE (ID = 2)
 */
#define FPGA_S_DEV_FCS_ID        2
#define REG_S_FCS_CONTROL_OFF    0x00
#define ADDR_S_FCS_CONTROL       FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_CONTROL_OFF)

#endif /* FPGA_REG_MAP_SPI_H */
