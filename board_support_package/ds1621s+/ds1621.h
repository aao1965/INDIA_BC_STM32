#ifndef DS1621_H
#define DS1621_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"
#include "board_support_package.h"

/* --- Configuration --- */
/* Выбор режима работы: 1 - Continuous (Непрерывный, по умолчанию), 0 - One-Shot (Одиночный) */
#define DS1621_MODE_CONTINUOUS    1

/* Использовать опрос бита DONE вместо фиксированной задержки 800мс (работает только в режиме One-Shot) */
#define DS1621_USE_DONE_POLLING   1 

/* --- I2C Hardware Pins --- */
#define I2C1_SCL_DS1621_Pin       GPIO_PIN_8
#define I2C1_SCL_DS1621_GPIO_Port GPIOB
#define I2C1_SDA_DS1621_Pin       GPIO_PIN_9
#define I2C1_SDA_DS1621_GPIO_Port GPIOB

#define DS1621_BIT_1SHOT        0x01
#define DS1621_ADDR             (0x48 << 1)

/* Commands & Registers */
#define DS1621_REG_TEMP         0xAA
#define DS1621_REG_CONFIG       0xAC
#define DS1621_REG_COUNT        0xA8
#define DS1621_REG_SLOPE        0xA9
#define DS1621_CMD_START        0xEE

/* Bitmasks */
#define DS1621_BIT_DONE         0x80

typedef enum {
    DS1621_OK = 0,
    DS1621_ERROR_I2C,
    DS1621_ERROR_INIT,
    DS1621_ERROR_BUSY,
    DS1621_TIMEOUT
} DS1621_Status;

/* Public API */
/**
 * @brief Initialize the library, create mutex and configure sensor.
 * @param hi2c_ptr Pointer to the HAL I2C handle (e.g., &hi2c1)
 * @return true if successful
 */
bool          DS1621_Init(I2C_HandleTypeDef *hi2c_ptr);

/**
 * @brief Perform one measurement cycle or read instantly based on mode.
 * @return Status code
 */
DS1621_Status DS1621_Update(void);

/**
 * @brief Calculate high-resolution temperature from registers.
 * @param out_temp Pointer to float to store the result
 * @return Status code
 */
DS1621_Status DS1621_GetTemperatureHighRes(float *out_temp);

/**
 * @brief Returns the last successfully measured temperature.
 */
float         DS1621_GetLastTemperature(void);

/**
 * @brief Returns the mutex handle for sharing the bus with other devices like AM1805.
 */
osMutexId_t   DS1621_GetI2CMutex(void);

/**
 * @brief Создает задачу RTOS для периодического опроса датчика.
 * @return osThreadId_t Хэндл созданной задачи (или NULL при ошибке).
 */
osThreadId_t  DS1621_CreateTask(void);

#endif /* DS1621_H */
