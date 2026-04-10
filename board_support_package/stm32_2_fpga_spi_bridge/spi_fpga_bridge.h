/*
 * spi_fpga_bridge.h
 * Кодировка: UTF-8.
 * Описание: Драйвер моста STM32-FPGA через SPI в блокирующем режиме.
 */

#ifndef __SPI_FPGA_BRIDGE_H
#define __SPI_FPGA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h" 
#include "cmsis_os.h"
#include "fpga_reg_map_spi.h"

/* --- Настройки протокола --- */
#define FPGA_SPI_WRITE_BIT      0x8000  // 15-й бит: 1=Запись, 0=Чтение
#define FPGA_SPI_TIMEOUT_MS     10      // Таймаут HAL SPI
#define FPGA_DEBUG_MAGIC_VAL    0x3F5A  // Ожидаемое значение из регистра CONST

/* --- Настройка пинов NSS (Chip Select) --- */
#define FPGA_NSS_PORT           SPI2_NSS_GPIO_Port
#define FPGA_NSS_PIN            SPI2_NSS_Pin

/* --- Структура управления мостом --- */
typedef struct {
    SPI_HandleTypeDef *hspi;
    osMutexId_t        spi_mtx;      // Рекурсивный мьютекс для защиты шины
    uint8_t tx_buf[5] __attribute__((aligned(4))); // Буфер передачи (40 бит)
    uint8_t rx_buf[5] __attribute__((aligned(4))); // Буфер приема (40 бит)
    bool    is_initialized;
} spi_bridge_t;

/* --- API Функции --- */

// Инициализация: создание мьютекса и проверка связи с ПЛИС
bool SPI_FPGA_Init(SPI_HandleTypeDef *hspi);

// Базовый ввод-вывод
bool SPI_FPGA_Write(uint16_t addr, uint16_t data);
bool SPI_FPGA_Read(uint16_t addr, uint16_t *data);

// Битовые операции (атомарные относительно шины, но RMW)
bool SPI_FPGA_SetBits(uint16_t addr, uint16_t mask);
bool SPI_FPGA_ClearBits(uint16_t addr, uint16_t mask);
bool SPI_FPGA_ToggleBits(uint16_t addr, uint16_t mask);
bool SPI_FPGA_TestBit(uint16_t addr, uint16_t mask);

#endif // __SPI_FPGA_BRIDGE_H
