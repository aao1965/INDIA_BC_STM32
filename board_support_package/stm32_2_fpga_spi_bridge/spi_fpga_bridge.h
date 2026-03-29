/*
 * spi_fpga_bridge.h
 * Кодировка: UTF-8. Библиотека моста STM32 - FPGA (Блокирующий режим / Polling).
 */

#ifndef __SPI_FPGA_BRIDGE_H
#define __SPI_FPGA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h" 
#include "cmsis_os.h"
#include "fpga_reg_map_spi.h"

/* --- НАСТРОЙКИ --- */
#define FPGA_SPI_WRITE_BIT      0x8000
#define FPGA_SPI_TIMEOUT_MS     10      // Таймаут для блокирующей функции HAL

// Настройка пинов NSS (выбор чипа)
#define FPGA_NSS_PORT           SPI2_NSS_GPIO_Port
#define FPGA_NSS_PIN            SPI2_NSS_Pin

/* --- СТРУКТУРА ДАННЫХ --- */
typedef struct {
    SPI_HandleTypeDef *hspi;
    osMutexId_t        spi_mtx;      // Рекурсивный мьютекс для многозадачности
    uint8_t tx_buf[4] __attribute__((aligned(4)));
    uint8_t rx_buf[4] __attribute__((aligned(4)));
} spi_bridge_t;

/* --- API ФУНКЦИИ --- */

// Инициализация моста
bool SPI_FPGA_Init(SPI_HandleTypeDef *hspi);

// Базовые функции записи и чтения
bool SPI_FPGA_Write(uint16_t addr, uint16_t data);
bool SPI_FPGA_Read(uint16_t addr, uint16_t *data);

// Функции манипуляции битами
bool SPI_FPGA_SetBits(uint16_t addr, uint16_t mask);
bool SPI_FPGA_ClearBits(uint16_t addr, uint16_t mask);
bool SPI_FPGA_ToggleBits(uint16_t addr, uint16_t mask);
bool SPI_FPGA_TestBit(uint16_t addr, uint16_t mask);

// Заглушка для совместимости (в Polling режиме не делает ничего)
void SPI_FPGA_NotifyTransferComplete(SPI_HandleTypeDef *hspi);

#endif // __SPI_FPGA_BRIDGE_H
