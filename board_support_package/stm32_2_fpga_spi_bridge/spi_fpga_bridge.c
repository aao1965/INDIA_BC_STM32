/*
 * spi_fpga_bridge.c
 * Кодировка: UTF-8.
 * Описание: Реализация обмена данными. Используется 5-байтовый протокол:
 * [Байт 0-1: Адрес+RW] -> [Байт 2: Dummy] -> [Байт 3-4: Данные]
 */

#include "spi_fpga_bridge.h"

static spi_bridge_t bridge = {0};

/**
 * @brief Управление линией NSS (выбор ПЛИС)
 */
static inline void FPGA_NSS_Select(void) {
    HAL_GPIO_WritePin(FPGA_NSS_PORT, FPGA_NSS_PIN, GPIO_PIN_RESET);
    // Короткая задержка для соблюдения tSLCH (Setup time)
    for(volatile int i=0; i<20; i++) { __NOP(); }
}

static inline void FPGA_NSS_Deselect(void) {
    // Задержка перед снятием CS (Hold time)
    for(volatile int i=0; i<20; i++) { __NOP(); }
    HAL_GPIO_WritePin(FPGA_NSS_PORT, FPGA_NSS_PIN, GPIO_PIN_SET);
}

/**
 * @brief Внутренняя функция транзакции (низкий уровень)
 */
static bool _InternalTransfer(uint16_t header, uint16_t data, uint16_t *rx_data) {
    if (!bridge.hspi) return false;

    // Формируем пакет (Big-Endian)
    bridge.tx_buf[0] = (uint8_t)(header >> 8);
    bridge.tx_buf[1] = (uint8_t)(header & 0xFF);
    bridge.tx_buf[2] = 0x00; // Dummy байт для компенсации задержки ПЛИС
    bridge.tx_buf[3] = (uint8_t)(data >> 8);
    bridge.tx_buf[4] = (uint8_t)(data & 0xFF);

    FPGA_NSS_Select();

    // Обмен 5 байтами (40 бит)
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(bridge.hspi,
                                                       bridge.tx_buf,
                                                       bridge.rx_buf,
                                                       5,
                                                       FPGA_SPI_TIMEOUT_MS);
    FPGA_NSS_Deselect();

    if (status != HAL_OK) return false;

    // Данные от ПЛИС всегда приходят в последних двух байтах транзакции
    if (rx_data) {
        *rx_data = (uint16_t)((bridge.rx_buf[3] << 8) | bridge.rx_buf[4]);
    }
    return true;
}

/**
 * @brief Инициализация моста
 */
bool SPI_FPGA_Init(SPI_HandleTypeDef *hspi) {
    bridge.hspi = hspi;
    bridge.is_initialized = false;
    FPGA_NSS_Deselect();

    // Создаем рекурсивный мьютекс (позволяет вложенные вызовы Read/Write)
    const osMutexAttr_t mtx_attr = { "spi_fpga_mtx", osMutexRecursive, NULL, 0 };
    bridge.spi_mtx = osMutexNew(&mtx_attr);

    if (bridge.spi_mtx == NULL) return false;

    // Проверка связи: читаем ID-регистр ПЛИС
    uint16_t magic = 0;
    if (SPI_FPGA_Read(ADDR_S_DEBUG_CONST, &magic)) {
        if (magic == FPGA_DEBUG_MAGIC_VAL) {
            bridge.is_initialized = true;
            return true;
        }
    }

    return false;
}

bool SPI_FPGA_Write(uint16_t addr, uint16_t data) {
    if (!bridge.is_initialized) return false;
    if (osMutexAcquire(bridge.spi_mtx, osWaitForever) != osOK) return false;

    bool res = _InternalTransfer(addr | FPGA_SPI_WRITE_BIT, data, NULL);

    osMutexRelease(bridge.spi_mtx);
    return res;
}

bool SPI_FPGA_Read(uint16_t addr, uint16_t *data) {
    // Для функции Init разрешаем чтение до полной установки флага is_initialized
    if (!bridge.hspi) return false;
    if (osMutexAcquire(bridge.spi_mtx, osWaitForever) != osOK) return false;

    bool res = _InternalTransfer(addr & ~FPGA_SPI_WRITE_BIT, 0x0000, data);

    osMutexRelease(bridge.spi_mtx);
    return res;
}

/* --- Составные операции (Read-Modify-Write) --- */

bool SPI_FPGA_SetBits(uint16_t addr, uint16_t mask) {
    if (osMutexAcquire(bridge.spi_mtx, osWaitForever) != osOK) return false;
    uint16_t val;
    bool res = (SPI_FPGA_Read(addr, &val) && SPI_FPGA_Write(addr, val | mask));
    osMutexRelease(bridge.spi_mtx);
    return res;
}

bool SPI_FPGA_ClearBits(uint16_t addr, uint16_t mask) {
    if (osMutexAcquire(bridge.spi_mtx, osWaitForever) != osOK) return false;
    uint16_t val;
    bool res = (SPI_FPGA_Read(addr, &val) && SPI_FPGA_Write(addr, val & ~mask));
    osMutexRelease(bridge.spi_mtx);
    return res;
}

bool SPI_FPGA_ToggleBits(uint16_t addr, uint16_t mask) {
    if (osMutexAcquire(bridge.spi_mtx, osWaitForever) != osOK) return false;
    uint16_t val;
    bool res = (SPI_FPGA_Read(addr, &val) && SPI_FPGA_Write(addr, val ^ mask));
    osMutexRelease(bridge.spi_mtx);
    return res;
}

bool SPI_FPGA_TestBit(uint16_t addr, uint16_t mask) {
    uint16_t val;
    if (SPI_FPGA_Read(addr, &val)) {
        return (val & mask) != 0;
    }
    return false;
}
