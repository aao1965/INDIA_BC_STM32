/*
 * spi_fpga_bridge.c
 * Кодировка: UTF-8. Реализация моста через Polling (блокирующий опрос).
 */

#include "spi_fpga_bridge.h"

static spi_bridge_t bridge;

/**
 * @brief Выбор чипа FPGA (NSS -> Low)
 */
static inline void FPGA_NSS_Select(void) {
    HAL_GPIO_WritePin(FPGA_NSS_PORT, FPGA_NSS_PIN, GPIO_PIN_RESET);
    for(volatile int i=0; i<20; i++);
}

/**
 * @brief Снятие выбора чипа FPGA (NSS -> High)
 */
static inline void FPGA_NSS_Deselect(void) {
    for(volatile int i=0; i<20; i++); 
    HAL_GPIO_WritePin(FPGA_NSS_PORT, FPGA_NSS_PIN, GPIO_PIN_SET);
}

/**
 * @brief Инициализация (только мьютекс)
 */
bool SPI_FPGA_Init(SPI_HandleTypeDef *hspi) {
    bridge.hspi = hspi;
    FPGA_NSS_Deselect();

    // Создаем рекурсивный мьютекс для защиты шины и атомарных операций
    const osMutexAttr_t mtx_attr = { "spi_mtx", osMutexRecursive, NULL, 0 };
    bridge.spi_mtx = osMutexNew(&mtx_attr);
    
    return (bridge.spi_mtx != NULL);
}

/**
 * @brief Внутренняя функция передачи данных (Polling)
 */
static bool _InternalTransfer(uint16_t header, uint16_t data, uint16_t *rx_data) {
    bridge.tx_buf[0] = (uint8_t)(header >> 8);
    bridge.tx_buf[1] = (uint8_t)(header & 0xFF);
    bridge.tx_buf[2] = (uint8_t)(data >> 8);
    bridge.tx_buf[3] = (uint8_t)(data & 0xFF);

    FPGA_NSS_Select();

    // Используем блокирующий метод HAL_SPI_TransmitReceive.
    // Процессор ждет завершения прямо здесь, не используя прерывания.
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(bridge.hspi, bridge.tx_buf, bridge.rx_buf, 4, FPGA_SPI_TIMEOUT_MS);

    FPGA_NSS_Deselect();

    if (status != HAL_OK) {
        return false;
    }

    if (rx_data) {
        *rx_data = (uint16_t)((bridge.rx_buf[2] << 8) | bridge.rx_buf[3]);
    }
    return true;
}

bool SPI_FPGA_Write(uint16_t addr, uint16_t data) {
    if (osMutexAcquire(bridge.spi_mtx, osWaitForever) != osOK) return false;
    bool res = _InternalTransfer(addr | FPGA_SPI_WRITE_BIT, data, NULL);
    osMutexRelease(bridge.spi_mtx);
    return res;
}

bool SPI_FPGA_Read(uint16_t addr, uint16_t *data) {
    if (osMutexAcquire(bridge.spi_mtx, osWaitForever) != osOK) return false;
    bool res = _InternalTransfer(addr & ~FPGA_SPI_WRITE_BIT, 0x0000, data);
    osMutexRelease(bridge.spi_mtx);
    return res;
}

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

/**
 * @brief Заглушка функции уведомления.
 * Больше не нужна для логики, но оставлена, чтобы не менять main.c
 */
void SPI_FPGA_NotifyTransferComplete(SPI_HandleTypeDef *hspi) {
    (void)hspi;
}
