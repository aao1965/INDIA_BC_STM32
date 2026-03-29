#include "fpga.h"
#include "fpga_reg_map.h"

/* Запись в регистр */
FSMC_Status_t fpga_write(uint16_t addr, uint16_t data) {
    if (addr > FPGA_MAX_ADDR) return FSMC_ERROR;
    return fsmc_write(FSMC_DEV_SPARTAN, (uint32_t)addr, data);
}

/* Чтение регистра */
FSMC_Status_t fpga_read(uint16_t addr, uint16_t *data) {
    if (addr > FPGA_MAX_ADDR || !data) return FSMC_ERROR;
    return fsmc_read(FSMC_DEV_SPARTAN, (uint32_t)addr, data);
}

/* Запись массива данных */
FSMC_Status_t fpga_write_buf(uint16_t addr, const uint16_t *p_data, uint32_t len) {
    if ((addr + len) > (FPGA_MAX_ADDR + 1) || !p_data) return FSMC_ERROR;
    return fsmc_write_buffer(FSMC_DEV_SPARTAN, (uint32_t)addr, p_data, len);
}

/* Чтение массива данных */
FSMC_Status_t fpga_read_buf(uint16_t addr, uint16_t *p_data, uint32_t len) {
    if ((addr + len) > (FPGA_MAX_ADDR + 1) || !p_data) return FSMC_ERROR;
    return fsmc_read_buffer(FSMC_DEV_SPARTAN, (uint32_t)addr, p_data, len);
}

/* Установка бит по маске */
FSMC_Status_t fpga_set_bit(uint16_t addr, uint16_t mask) {
    if (addr > FPGA_MAX_ADDR) return FSMC_ERROR;
    return fsmc_set_bit(FSMC_DEV_SPARTAN, (uint32_t)addr, mask);
}

/* Сброс бит по маске */
FSMC_Status_t fpga_clr_bit(uint16_t addr, uint16_t mask) {
    if (addr > FPGA_MAX_ADDR) return FSMC_ERROR;
    return fsmc_clr_bit(FSMC_DEV_SPARTAN, (uint32_t)addr, mask);
}

/* Инверсия бит (XOR) */
FSMC_Status_t fpga_tgl_bit(uint16_t addr, uint16_t mask) {
    if (addr > FPGA_MAX_ADDR) return FSMC_ERROR;
    return fsmc_tgl_bit(FSMC_DEV_SPARTAN, (uint32_t)addr, mask);
}

/* Проверка бит (1 - установлен, 0 - нет) */
uint8_t fpga_test_bit(uint16_t addr, uint16_t mask) {
    if (addr > FPGA_MAX_ADDR) return 0;
    return fsmc_test_bit(FSMC_DEV_SPARTAN, (uint32_t)addr, mask);
}

/* Базовый тест связи */
#include "fpga.h"
#include "fpga_reg_map.h"

/**
 * @brief Полный тест связи с ПЛИС (CONST + ECHO)
 * Проверяет наличие ПЛИС и отсутствие КЗ/обрывов на шине данных.
 */
FSMC_Status_t fpga_test_link(void) {
    uint16_t read_val = 0;

    // 1. Проверка константы (проверка адресации и чтения)
    if (fpga_read(ADDR_DEBUG_CONST, &read_val) != FSMC_OK) return FSMC_TIMEOUT;
    if (read_val != 0x3F5A) return FSMC_ERROR;

    // 2. Тест "Шахматка" (0xAAAA и 0x5555)
    uint16_t patterns[] = {0xAAAA, 0x5555};
    for (int i = 0; i < 2; i++) {
        if (fpga_write(ADDR_DEBUG_FEEDBACK, patterns[i]) != FSMC_OK) return FSMC_TIMEOUT;
        fpga_read(ADDR_DEBUG_FEEDBACK, &read_val);
        if (read_val != patterns[i]) return FSMC_ERROR;
    }

    // 3. Тест "Бегущая единица" (проверка каждой линии D0..D15 на КЗ)
    for (int i = 0; i < 16; i++) {
        uint16_t bit_pattern = (1 << i);
        fpga_write(ADDR_DEBUG_FEEDBACK, bit_pattern);
        fpga_read(ADDR_DEBUG_FEEDBACK, &read_val);
        if (read_val != bit_pattern) return FSMC_ERROR;
    }

    // Восстанавливаем эхо-регистр в 0 по завершении теста
    fpga_write(ADDR_DEBUG_FEEDBACK, 0x0000);

    return FSMC_OK;
}

