#ifndef DS1621_H
#define DS1621_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"
#include "board_support_package.h"

/* --- Configuration --- */
/* Uncomment to use DONE bit polling instead of a fixed 800ms delay */
#define DS1621_USE_DONE_POLLING   1 

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
 * @brief Perform one measurement cycle (Start -> Wait/Poll -> Read).
 * @return Status code
 */
DS1621_Status DS1621_Update(void);

/**
 * @brief Calculate high-resolution temperature from registers.
 * @param temperature Pointer to store the result
 * @return Status code
 */
DS1621_Status DS1621_GetTemperatureHighRes(float *temperature);

/**
 * @brief Thread-safe getter for the last valid temperature.
 * @return Last measured temperature in Celsius
 */
float         DS1621_GetLastTemperature(void);

/* Shared Resource Access */
/**
 * @brief Returns the mutex handle to allow sharing the I2C bus with AM1805.
 * @return Mutex ID
 */
osMutexId_t   DS1621_GetI2CMutex(void);

/* Task Management */
osThreadId_t  DS1621_CreateTask(void);

#endif /* DS1621_H */

