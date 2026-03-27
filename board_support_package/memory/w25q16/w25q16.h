#ifndef MEMORY_W25Q16_W25Q16_H_
#define MEMORY_W25Q16_W25Q16_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "string.h"
#include "pin_mgmt.h"

// Memory sizes (Updated for W25Q16)
#define W25Q16_FLASH_SIZE          0x200000     // 2 MB
#define W25Q16_SECTOR_SIZE         0x1000       // 4 KB
#define W25Q16_BLOCK_SIZE          0x10000      // 64 KB
#define W25Q16_PAGE_SIZE           0x100        // 256 B

// Commands
#define W25Q16_CMD_WRITE_ENABLE    0x06
#define W25Q16_CMD_WRITE_DISABLE   0x04
#define W25Q16_CMD_READ_DATA       0x03
#define W25Q16_CMD_FAST_READ       0x0B
#define W25Q16_CMD_PAGE_PROGRAM    0x02
#define W25Q16_CMD_SECTOR_ERASE    0x20
#define W25Q16_CMD_BLOCK_ERASE     0xD8
#define W25Q16_CMD_CHIP_ERASE      0xC7
#define W25Q16_CMD_READ_STATUS1    0x05
#define W25Q16_CMD_READ_STATUS2    0x35
#define W25Q16_CMD_READ_STATUS3    0x15
#define W25Q16_CMD_WRITE_STATUS1   0x01
#define W25Q16_CMD_WRITE_STATUS2   0x31
#define W25Q16_CMD_WRITE_STATUS3   0x11
#define W25Q16_CMD_READ_JEDEC_ID   0x9F
#define W25Q16_CMD_READ_UID        0x4B
#define W25Q16_CMD_POWER_DOWN      0xB9
#define W25Q16_CMD_RELEASE_PD      0xAB

// Status registers
#define W25Q16_STATUS_BUSY         0x01
#define W25Q16_STATUS_WEL          0x02

// JEDEC ID Configuration
#define W25Q16_MANUFACTURER_ID     0xEF
#define W25Q16_MEMORY_TYPE         0x40
#define W25Q16_CAPACITY_ID         0x15

// Timeouts
#define W25Q16_TIMEOUT_MS          1000
#define W25Q16_BUSY_TIMEOUT_MS     100
#define W25Q16_TIMEOUT_CHIP_ERASE_S 40
#define W25Q16_TIMEOUT_CHIP_WRITE_S 10

// Mutex attributes
#define W25Q16_MUTEX_ATTR { \
    .name = "W25Q16_Mutex", \
    .attr_bits = osMutexRecursive | osMutexPrioInherit, \
    .cb_mem = NULL, \
    .cb_size = 0 \
}

#define W25Q16_UID_SIZE            8

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    uint8_t use_pin_mgmt;
    uint8_t initialized;
    uint32_t capacity;
    osMutexId_t spi_mutex;
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_id;
    uint8_t unique_id[W25Q16_UID_SIZE];
    uint8_t jedec_valid;
    uint8_t uid_valid;
} W25Q16_Handle_t;

typedef struct {
    uint32_t address;
    uint8_t *data;
    uint32_t size;
    osStatus_t status;
} W25Q16_Operation_t;

/* --- Public Functions --- */
osStatus_t W25Q16_Init(W25Q16_Handle_t *flash, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin);
osStatus_t W25Q16_Init_Ex(W25Q16_Handle_t *flash, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t use_pin_mgmt);
osStatus_t W25Q16_Init_With_PinMgmt(W25Q16_Handle_t *flash, SPI_HandleTypeDef *hspi);
osStatus_t W25Q16_DeInit(W25Q16_Handle_t *flash);

osStatus_t W25Q16_ReadData(W25Q16_Handle_t *flash, uint32_t address, uint8_t *data, uint32_t size);
osStatus_t W25Q16_FastRead(W25Q16_Handle_t *flash, uint32_t address, uint8_t *data, uint32_t size);
osStatus_t W25Q16_PageProgram(W25Q16_Handle_t *flash, uint32_t address, const uint8_t *data, uint32_t size);
osStatus_t W25Q16_SectorErase(W25Q16_Handle_t *flash, uint32_t address);
osStatus_t W25Q16_BlockErase(W25Q16_Handle_t *flash, uint32_t address);
osStatus_t W25Q16_ChipErase(W25Q16_Handle_t *flash);

osStatus_t W25Q16_ReadJEDECID(W25Q16_Handle_t *flash);
osStatus_t W25Q16_GetJEDECID(W25Q16_Handle_t *flash, uint8_t *manufacturer, uint8_t *memory_type, uint8_t *capacity);
uint8_t W25Q16_IsJEDECValid(W25Q16_Handle_t *flash);

osStatus_t W25Q16_ReadUID(W25Q16_Handle_t *flash);
osStatus_t W25Q16_GetUID(W25Q16_Handle_t *flash, uint8_t *uid);
const uint8_t* W25Q16_GetUIDPtr(W25Q16_Handle_t *flash);
uint8_t W25Q16_IsUIDValid(W25Q16_Handle_t *flash);

osStatus_t W25Q16_ReadStatusRegister(W25Q16_Handle_t *flash, uint8_t reg_num, uint8_t *status);
osStatus_t W25Q16_WriteStatusRegister(W25Q16_Handle_t *flash, uint8_t reg_num, uint8_t status);
osStatus_t W25Q16_WaitForReady(W25Q16_Handle_t *flash, uint32_t timeout);
osStatus_t W25Q16_WriteEnable(W25Q16_Handle_t *flash);
osStatus_t W25Q16_WriteDisable(W25Q16_Handle_t *flash);

osStatus_t W25Q16_ReadSector(W25Q16_Handle_t *flash, uint32_t sector, uint8_t *data);
osStatus_t W25Q16_WriteSector(W25Q16_Handle_t *flash, uint32_t sector, const uint8_t *data);
osStatus_t W25Q16_EraseSector(W25Q16_Handle_t *flash, uint32_t sector);
osStatus_t W25Q16_EraseBlock(W25Q16_Handle_t *flash, uint32_t block);

void W25Q16_Task(void *argument);

#ifdef __cplusplus
}
#endif
#endif
