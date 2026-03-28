#include "fsmc_memory.h"

static osMutexId_t fsmc_mtx;
const uint32_t fsmc_base_addrs[FSMC_DEV_COUNT] = {FSMC_ADDR_NE1, FSMC_ADDR_NE2, FSMC_ADDR_NE3};

/* Инициализация рекурсивного мьютекса */
FSMC_Status_t fsmc_init(void) {
    const osMutexAttr_t attr = {"fsmc_mtx", osMutexRecursive | osMutexPrioInherit, NULL, 0};
    fsmc_mtx = osMutexNew(&attr);
    return (fsmc_mtx != NULL) ? FSMC_OK : FSMC_ERROR;
}

/* Блокировка шины (ожидание вечно) */
FSMC_Status_t fsmc_lock(void) {
    return (osMutexAcquire(fsmc_mtx, osWaitForever) == osOK) ? FSMC_OK : FSMC_TIMEOUT;
}

/* Разблокировка шины */
void fsmc_unlock(void) { osMutexRelease(fsmc_mtx); }

/* Одиночная запись с блокировкой */
FSMC_Status_t fsmc_write(FSMC_Device_t dev, uint32_t addr, uint16_t data) {
    if (fsmc_lock() != FSMC_OK) return FSMC_TIMEOUT;
    fsmc_write_fast(dev, addr, data);
    fsmc_unlock();
    return FSMC_OK;
}

/* Одиночное чтение с блокировкой */
FSMC_Status_t fsmc_read(FSMC_Device_t dev, uint32_t addr, uint16_t *data) {
    if (fsmc_lock() != FSMC_OK) return FSMC_TIMEOUT;
    *data = fsmc_read_fast(dev, addr);
    fsmc_unlock();
    return FSMC_OK;
}

/* Запись массива данных */
FSMC_Status_t fsmc_write_buffer(FSMC_Device_t dev, uint32_t addr, const uint16_t *p_data, uint32_t count) {
    if (!p_data) return FSMC_ERROR;
    if (fsmc_lock() != FSMC_OK) return FSMC_TIMEOUT;
    volatile uint16_t *dst = (volatile uint16_t*)(fsmc_base_addrs[dev]) + addr;
    while(count--) *dst++ = *p_data++;
    fsmc_unlock();
    return FSMC_OK;
}

/* Чтение массива данных */
FSMC_Status_t fsmc_read_buffer(FSMC_Device_t dev, uint32_t addr, uint16_t *p_data, uint32_t count) {
    if (!p_data) return FSMC_ERROR;
    if (fsmc_lock() != FSMC_OK) return FSMC_TIMEOUT;
    volatile uint16_t *src = (volatile uint16_t*)(fsmc_base_addrs[dev]) + addr;
    while(count--) *p_data++ = *src++;
    fsmc_unlock();
    return FSMC_OK;
}

/* Установка бит по маске (RMW) */
FSMC_Status_t fsmc_set_bit(FSMC_Device_t dev, uint32_t addr, uint16_t mask) {
    if (fsmc_lock() != FSMC_OK) return FSMC_TIMEOUT;
    fsmc_write_fast(dev, addr, fsmc_read_fast(dev, addr) | mask);
    fsmc_unlock();
    return FSMC_OK;
}

/* Сброс бит по маске (RMW) */
FSMC_Status_t fsmc_clr_bit(FSMC_Device_t dev, uint32_t addr, uint16_t mask) {
    if (fsmc_lock() != FSMC_OK) return FSMC_TIMEOUT;
    fsmc_write_fast(dev, addr, fsmc_read_fast(dev, addr) & ~mask);
    fsmc_unlock();
    return FSMC_OK;
}

/* Инверсия бит по маске (RMW) */
FSMC_Status_t fsmc_tgl_bit(FSMC_Device_t dev, uint32_t addr, uint16_t mask) {
    if (fsmc_lock() != FSMC_OK) return FSMC_TIMEOUT;
    fsmc_write_fast(dev, addr, fsmc_read_fast(dev, addr) ^ mask);
    fsmc_unlock();
    return FSMC_OK;
}

/* Проверка бит (1 - установлен, 0 - сброшен или ошибка) */
uint8_t fsmc_test_bit(FSMC_Device_t dev, uint32_t addr, uint16_t mask) {
    uint16_t val;
    if (fsmc_read(dev, addr, &val) != FSMC_OK) return 0;
    return (val & mask) ? 1 : 0;
}