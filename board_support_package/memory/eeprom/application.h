#ifndef MEMORY_APPLICATION_H_
#define MEMORY_APPLICATION_H_

#include "board_support_package.h"

// Адрес 5-го сектора Flash-памяти (Начало пользовательского приложения)
#define APPLICATION_ADDRESS         0x08020000

// 128 kB (Размер одного сектора: с 5 по 11 на STM32F4)
#define FLASH_CPU_SECTOR_SIZE       (128 * 1024)
#define FLASH_CPU_START_ADDRESS     APPLICATION_ADDRESS 
#define FLASH_CPU_ADR               FLASH_CPU_START_ADDRESS

// --- НАСТРОЙКА РАЗМЕРА ПРОГРАММЫ ---
// Укажите, сколько секторов мы отдаем под боевую программу (начиная с Сектора 5)
// Например: 2 сектора = 256 КБ
#define FLASH_CPU_SECTORS_COUNT     1

// Размер страницы обмена DSPA (1024 байта)
#define FLASH_CPU_PAGE_SIZE         1024

// Вычисляемый общий размер выделенной памяти
#define FLASH_CPU_SIZE              (FLASH_CPU_SECTOR_SIZE * FLASH_CPU_SECTORS_COUNT) 
#define FLASH_CPU_ADR_HIGH          (FLASH_CPU_START_ADDRESS + FLASH_CPU_SIZE - 1)

#define CLEAR_FLAG                  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR)

// Стартовый сектор
#define APPL_SECTOR                 FLASH_SECTOR_5

// Прототипы функций драйвера
void eeprom_memory_read_buffer(uint8_t *dst, const uint8_t *src, uint32_t len);
bool eeprom_memory_erase(uint32_t sector);
bool eeprom_memory_write_words(const uint32_t *p_data, uint32_t addr, uint32_t words_count);

/**
 * @brief Проверяет наличие прошивки и валидность её CRC32.
 * @return true, если прошивка валидна и можно делать jump.
 * @return false, если прошивки нет, или CRC не совпадает.
 */
bool check_application(void);

#endif /* MEMORY_APPLICATION_H_ */
