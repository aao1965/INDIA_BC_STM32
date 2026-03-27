#include "w25q16.h"

static osMutexId_t static_spi_mutex = NULL;

/* --- Private Functions --- */

static void W25Q16_CS_Select(W25Q16_Handle_t *flash) {
    if (flash->use_pin_mgmt) PIN_Reset(&pin_nss_xc6);
    else HAL_GPIO_WritePin(flash->cs_port, flash->cs_pin, GPIO_PIN_RESET);
}

static void W25Q16_CS_Deselect(W25Q16_Handle_t *flash) {
    if (flash->use_pin_mgmt) PIN_Set(&pin_nss_xc6);
    else HAL_GPIO_WritePin(flash->cs_port, flash->cs_pin, GPIO_PIN_SET);
}

static osStatus_t W25Q16_LockMutex(W25Q16_Handle_t *flash, uint32_t timeout) {
    if (flash == NULL || flash->spi_mutex == NULL) return osErrorParameter;
    return osMutexAcquire(flash->spi_mutex, timeout);
}

static osStatus_t W25Q16_UnlockMutex(W25Q16_Handle_t *flash) {
    if (flash == NULL || flash->spi_mutex == NULL) return osErrorParameter;
    return osMutexRelease(flash->spi_mutex);
}

static osStatus_t W25Q16_TransmitReceive(W25Q16_Handle_t *flash, uint8_t *tx_data, uint8_t *rx_data, uint16_t size) {
    osStatus_t status = W25Q16_LockMutex(flash, osWaitForever);
    if (status != osOK) return status;
    W25Q16_CS_Select(flash);
    HAL_StatusTypeDef hal_status = HAL_SPI_TransmitReceive(flash->hspi, tx_data, rx_data, size, HAL_MAX_DELAY);
    W25Q16_CS_Deselect(flash);
    W25Q16_UnlockMutex(flash);
    return (hal_status == HAL_OK) ? osOK : osError;
}

static osStatus_t W25Q16_Transmit(W25Q16_Handle_t *flash, uint8_t *data, uint16_t size) {
    osStatus_t status = W25Q16_LockMutex(flash, osWaitForever);
    if (status != osOK) return status;
    W25Q16_CS_Select(flash);
    HAL_StatusTypeDef hal_status = HAL_SPI_Transmit(flash->hspi, data, size, HAL_MAX_DELAY);
    W25Q16_CS_Deselect(flash);
    W25Q16_UnlockMutex(flash);
    return (hal_status == HAL_OK) ? osOK : osError;
}

/* --- ID Functions --- */

osStatus_t W25Q16_ReadJEDECID(W25Q16_Handle_t *flash) {
    if (flash == NULL) return osErrorParameter;
    uint8_t tx_data[4] = {W25Q16_CMD_READ_JEDEC_ID, 0, 0, 0};
    uint8_t rx_data[4];
    osStatus_t status = W25Q16_TransmitReceive(flash, tx_data, rx_data, 4);
    if (status != osOK) { flash->jedec_valid = 0; return status; }
    flash->manufacturer_id = rx_data[1];
    flash->memory_type = rx_data[2];
    flash->capacity_id = rx_data[3];
    flash->jedec_valid = 1;
    return osOK;
}

osStatus_t W25Q16_GetJEDECID(W25Q16_Handle_t *flash, uint8_t *manufacturer, uint8_t *memory_type, uint8_t *capacity) {
    if (flash == NULL || !flash->jedec_valid) return osErrorResource;
    if (manufacturer) *manufacturer = flash->manufacturer_id;
    if (memory_type) *memory_type = flash->memory_type;
    if (capacity) *capacity = flash->capacity_id;
    return osOK;
}

uint8_t W25Q16_IsJEDECValid(W25Q16_Handle_t *flash) { return (flash != NULL) ? flash->jedec_valid : 0; }

osStatus_t W25Q16_ReadUID(W25Q16_Handle_t *flash) {
    if (flash == NULL) return osErrorParameter;
    uint8_t tx_data[5] = {W25Q16_CMD_READ_UID, 0, 0, 0, 0};
    uint8_t rx_data[13];
    osStatus_t status = W25Q16_TransmitReceive(flash, tx_data, rx_data, 13);
    if (status != osOK) { flash->uid_valid = 0; return status; }
    memcpy(flash->unique_id, &rx_data[5], W25Q16_UID_SIZE);
    flash->uid_valid = 1;
    return osOK;
}

osStatus_t W25Q16_GetUID(W25Q16_Handle_t *flash, uint8_t *uid) {
    if (flash == NULL || uid == NULL || !flash->uid_valid) return osErrorResource;
    memcpy(uid, flash->unique_id, W25Q16_UID_SIZE);
    return osOK;
}

const uint8_t* W25Q16_GetUIDPtr(W25Q16_Handle_t *flash) { return (flash != NULL) ? flash->unique_id : NULL; }
uint8_t W25Q16_IsUIDValid(W25Q16_Handle_t *flash) { return (flash != NULL) ? flash->uid_valid : 0; }

/* --- Initialization --- */

osStatus_t W25Q16_Init_Ex(W25Q16_Handle_t *flash, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t use_pin_mgmt) {
    if (flash == NULL || hspi == NULL) return osErrorParameter;
    memset(flash, 0, sizeof(W25Q16_Handle_t));
    flash->hspi = hspi; flash->cs_port = cs_port; flash->cs_pin = cs_pin;
    flash->use_pin_mgmt = use_pin_mgmt; flash->capacity = W25Q16_FLASH_SIZE;

    if (use_pin_mgmt && SPI_Bus_Acquire_For_STM32() != osOK) return osErrorResource;
    if (static_spi_mutex == NULL) {
        const osMutexAttr_t mutex_attr = W25Q16_MUTEX_ATTR;
        static_spi_mutex = osMutexNew(&mutex_attr);
    }
    if (static_spi_mutex == NULL) return osErrorResource;
    flash->spi_mutex = static_spi_mutex;

    if (W25Q16_ReadJEDECID(flash) != osOK || flash->manufacturer_id != W25Q16_MANUFACTURER_ID ||
        flash->capacity_id != W25Q16_CAPACITY_ID) {
        if (use_pin_mgmt) SPI_Bus_Release_To_FPGA();
        return osError;
    }
    flash->initialized = 1;
    return osOK;
}

osStatus_t W25Q16_Init(W25Q16_Handle_t *flash, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin) {
    return W25Q16_Init_Ex(flash, hspi, cs_port, cs_pin, 0);
}

osStatus_t W25Q16_Init_With_PinMgmt(W25Q16_Handle_t *flash, SPI_HandleTypeDef *hspi) {
    return W25Q16_Init_Ex(flash, hspi, NSS_STM32_XC6_GPIO_Port, NSS_STM32_XC6_Pin, 1);
}

osStatus_t W25Q16_DeInit(W25Q16_Handle_t *flash) {
    if (flash == NULL || !flash->initialized) return osErrorParameter;
    if (flash->use_pin_mgmt) SPI_Bus_Release_To_FPGA();
    memset(flash, 0, sizeof(W25Q16_Handle_t));
    return osOK;
}

/* --- Utility Functions --- */

osStatus_t W25Q16_WaitForReady(W25Q16_Handle_t *flash, uint32_t timeout) {
    uint32_t start = osKernelGetTickCount();
    uint8_t status;
    do {
        if (W25Q16_ReadStatusRegister(flash, 1, &status) != osOK) return osError;
        if (!(status & W25Q16_STATUS_BUSY)) return osOK;
        osDelay(1);
    } while ((osKernelGetTickCount() - start) < timeout);
    return osErrorTimeout;
}

osStatus_t W25Q16_WriteEnable(W25Q16_Handle_t *flash) {
    uint8_t cmd = W25Q16_CMD_WRITE_ENABLE;
    return W25Q16_Transmit(flash, &cmd, 1);
}

osStatus_t W25Q16_WriteDisable(W25Q16_Handle_t *flash) {
    uint8_t cmd = W25Q16_CMD_WRITE_DISABLE;
    return W25Q16_Transmit(flash, &cmd, 1);
}

osStatus_t W25Q16_ReadStatusRegister(W25Q16_Handle_t *flash, uint8_t reg_num, uint8_t *status) {
    uint8_t cmd;
    switch (reg_num) {
        case 1: cmd = W25Q16_CMD_READ_STATUS1; break;
        case 2: cmd = W25Q16_CMD_READ_STATUS2; break;
        case 3: cmd = W25Q16_CMD_READ_STATUS3; break;
        default: return osErrorParameter;
    }
    uint8_t tx[2] = {cmd, 0}, rx[2];
    osStatus_t res = W25Q16_TransmitReceive(flash, tx, rx, 2);
    if (res == osOK) *status = rx[1];
    return res;
}

osStatus_t W25Q16_WriteStatusRegister(W25Q16_Handle_t *flash, uint8_t reg_num, uint8_t status) {
    return osError; // Needs implementation if needed
}

/* --- Data Operations --- */

osStatus_t W25Q16_ReadData(W25Q16_Handle_t *flash, uint32_t address, uint8_t *data, uint32_t size) {
    if (!flash || !flash->initialized || address + size > flash->capacity) return osErrorParameter;
    uint8_t header[4] = {W25Q16_CMD_READ_DATA, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF};
    if (W25Q16_LockMutex(flash, osWaitForever) != osOK) return osError;
    W25Q16_CS_Select(flash);
    HAL_SPI_Transmit(flash->hspi, header, 4, HAL_MAX_DELAY);
    HAL_StatusTypeDef h_res = HAL_SPI_Receive(flash->hspi, data, size, HAL_MAX_DELAY);
    W25Q16_CS_Deselect(flash);
    W25Q16_UnlockMutex(flash);
    return (h_res == HAL_OK) ? osOK : osError;
}

osStatus_t W25Q16_FastRead(W25Q16_Handle_t *flash, uint32_t address, uint8_t *data, uint32_t size) {
    if (!flash || !flash->initialized || address + size > flash->capacity) return osErrorParameter;
    uint8_t header[5] = {W25Q16_CMD_FAST_READ, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF, 0};
    if (W25Q16_LockMutex(flash, osWaitForever) != osOK) return osError;
    W25Q16_CS_Select(flash);
    HAL_SPI_Transmit(flash->hspi, header, 5, HAL_MAX_DELAY);
    HAL_StatusTypeDef h_res = HAL_SPI_Receive(flash->hspi, data, size, HAL_MAX_DELAY);
    W25Q16_CS_Deselect(flash);
    W25Q16_UnlockMutex(flash);
    return (h_res == HAL_OK) ? osOK : osError;
}

osStatus_t W25Q16_PageProgram(W25Q16_Handle_t *flash, uint32_t address, const uint8_t *data, uint32_t size) {
    if (!flash || size > W25Q16_PAGE_SIZE || address + size > flash->capacity) return osErrorParameter;
    if (W25Q16_WaitForReady(flash, W25Q16_BUSY_TIMEOUT_MS) != osOK) return osError;
    W25Q16_WriteEnable(flash);
    uint8_t header[4] = {W25Q16_CMD_PAGE_PROGRAM, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF};
    if (W25Q16_LockMutex(flash, osWaitForever) != osOK) return osError;
    W25Q16_CS_Select(flash);
    HAL_SPI_Transmit(flash->hspi, header, 4, HAL_MAX_DELAY);
    HAL_StatusTypeDef h_res = HAL_SPI_Transmit(flash->hspi, (uint8_t*)data, size, HAL_MAX_DELAY);
    W25Q16_CS_Deselect(flash);
    W25Q16_UnlockMutex(flash);
    return (h_res == HAL_OK) ? W25Q16_WaitForReady(flash, W25Q16_BUSY_TIMEOUT_MS) : osError;
}

osStatus_t W25Q16_SectorErase(W25Q16_Handle_t *flash, uint32_t address) {
    if (!flash || address >= flash->capacity) return osErrorParameter;
    address &= ~(W25Q16_SECTOR_SIZE - 1);
    if (W25Q16_WaitForReady(flash, W25Q16_BUSY_TIMEOUT_MS) != osOK) return osError;
    W25Q16_WriteEnable(flash);
    uint8_t header[4] = {W25Q16_CMD_SECTOR_ERASE, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF};
    if (W25Q16_Transmit(flash, header, 4) != osOK) return osError;
    return W25Q16_WaitForReady(flash, W25Q16_TIMEOUT_MS);
}

osStatus_t W25Q16_BlockErase(W25Q16_Handle_t *flash, uint32_t address) {
    if (!flash || address >= flash->capacity) return osErrorParameter;
    address &= ~(W25Q16_BLOCK_SIZE - 1);
    if (W25Q16_WaitForReady(flash, W25Q16_BUSY_TIMEOUT_MS) != osOK) return osError;
    W25Q16_WriteEnable(flash);
    uint8_t header[4] = {W25Q16_CMD_BLOCK_ERASE, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF};
    if (W25Q16_Transmit(flash, header, 4) != osOK) return osError;
    return W25Q16_WaitForReady(flash, W25Q16_TIMEOUT_MS);
}

osStatus_t W25Q16_ChipErase(W25Q16_Handle_t *flash) {
    if (!flash) return osErrorParameter;
    if (W25Q16_WaitForReady(flash, W25Q16_BUSY_TIMEOUT_MS) != osOK) return osError;
    W25Q16_WriteEnable(flash);
    uint8_t cmd = W25Q16_CMD_CHIP_ERASE;
    if (W25Q16_Transmit(flash, &cmd, 1) != osOK) return osError;
    return W25Q16_WaitForReady(flash, W25Q16_TIMEOUT_CHIP_ERASE_S * 1000);
}

/* --- Advanced Functions --- */

osStatus_t W25Q16_ReadSector(W25Q16_Handle_t *flash, uint32_t sector, uint8_t *data) {
    if (sector * W25Q16_SECTOR_SIZE >= flash->capacity) return osErrorParameter;
    return W25Q16_ReadData(flash, sector * W25Q16_SECTOR_SIZE, data, W25Q16_SECTOR_SIZE);
}

osStatus_t W25Q16_WriteSector(W25Q16_Handle_t *flash, uint32_t sector, const uint8_t *data) {
    uint32_t addr = sector * W25Q16_SECTOR_SIZE;
    if (W25Q16_SectorErase(flash, addr) != osOK) return osError;
    for (uint32_t i = 0; i < W25Q16_SECTOR_SIZE; i += W25Q16_PAGE_SIZE) {
        if (W25Q16_PageProgram(flash, addr + i, data + i, W25Q16_PAGE_SIZE) != osOK) return osError;
    }
    return osOK;
}

osStatus_t W25Q16_EraseSector(W25Q16_Handle_t *flash, uint32_t sector) {
    return W25Q16_SectorErase(flash, sector * W25Q16_SECTOR_SIZE);
}

osStatus_t W25Q16_EraseBlock(W25Q16_Handle_t *flash, uint32_t block) {
    return W25Q16_BlockErase(flash, block * W25Q16_BLOCK_SIZE);
}

void W25Q16_Task(void *argument) { /* Placeholder */ }
