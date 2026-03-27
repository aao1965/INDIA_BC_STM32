#include "pin_mgmt.h"

static osMutexId_t pin_mutex = NULL;

/* --- Descriptor Initialization --- */
const Pin_Descriptor_t pin_fpga_done     = {PMGT_DONE_PORT, PMGT_DONE_PIN, "DONE"};
const Pin_Descriptor_t pin_reset_xs6     = {PMGT_RESET_PORT, PMGT_RESET_PIN, "RESET_XS6"};
const Pin_Descriptor_t pin_sel_mem_stm32 = {PMGT_SEL_MEM_PORT, PMGT_SEL_MEM_PIN, "SEL_MEM"};
const Pin_Descriptor_t pin_nss_spi2      = {PMGT_NSS_SPI2_PORT, PMGT_NSS_SPI2_PIN, "NSS_SPI2"};
const Pin_Descriptor_t pin_nss_xc6       = {PMGT_NSS_XC6_PORT, PMGT_NSS_XC6_PIN, "NSS_XC6"};
const Pin_Descriptor_t pin_stlink_gnd    = {PMGT_STLINK_GND_PORT, PMGT_STLINK_GND_PIN, "STLINK_GND"};

/* Fixed typo in GPIO_0 mapping */
const Pin_Descriptor_t pin_w_up_uart     = {W_UP_EXTI14_GPIO_Port, W_UP_EXTI14_Pin, "W_UP_UART"};
const Pin_Descriptor_t pin_gpio_0        = {GPIO_0_GPIO_Port, GPIO_0_Pin, "GPIO_0"};
const Pin_Descriptor_t pin_gpio_1        = {GPIO_1_GPIO_Port, GPIO_1_Pin, "GPIO_1"};
const Pin_Descriptor_t pin_tp0           = {TP0_GPIO_Port, TP0_Pin, "TP0"};
const Pin_Descriptor_t pin_tp1           = {TP1_GPIO_Port, TP1_Pin, "TP1"};

osStatus_t PIN_Mgmt_Init(void) {
    const osMutexAttr_t attr = {"pin_mutex", osMutexRecursive | osMutexPrioInherit, NULL, 0};
    pin_mutex = osMutexNew(&attr);

    /* Set Initial Hardware States */
    HAL_GPIO_WritePin(PMGT_SEL_MEM_PORT, PMGT_SEL_MEM_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PMGT_RESET_PORT, PMGT_RESET_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PMGT_NSS_XC6_PORT, PMGT_NSS_XC6_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PMGT_NSS_SPI2_PORT, PMGT_NSS_SPI2_PIN, GPIO_PIN_SET);

    return (pin_mutex != NULL) ? osOK : osError;
}

/* --- SYSTEM FUNCTIONS --- */

osStatus_t SPI_Bus_Acquire_For_STM32(void) {
    HAL_GPIO_WritePin(PMGT_SEL_MEM_PORT, PMGT_SEL_MEM_PIN, GPIO_PIN_RESET);
    for(volatile int i = 0; i < 40; i++);
    return osOK;
}

osStatus_t SPI_Bus_Release_To_FPGA(void) {
    HAL_GPIO_WritePin(PMGT_SEL_MEM_PORT, PMGT_SEL_MEM_PIN, GPIO_PIN_SET);
    return osOK;
}

osStatus_t FPGA_System_Restart(uint32_t timeout_ms) {
    HAL_GPIO_WritePin(PMGT_SEL_MEM_PORT, PMGT_SEL_MEM_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PMGT_RESET_PORT, PMGT_RESET_PIN, GPIO_PIN_RESET);
    osDelay(2);
    HAL_GPIO_WritePin(PMGT_RESET_PORT, PMGT_RESET_PIN, GPIO_PIN_SET);

    uint32_t start = osKernelGetTickCount();
    while ((osKernelGetTickCount() - start) < timeout_ms) {
        if (HAL_GPIO_ReadPin(PMGT_DONE_PORT, PMGT_DONE_PIN) == GPIO_PIN_SET) return osOK;
        osDelay(1);
    }
    return osErrorTimeout;
}

void	FPGA_Reset(void){
	HAL_GPIO_WritePin(PMGT_RESET_PORT, PMGT_RESET_PIN, GPIO_PIN_RESET);
	osDelay(2);
	HAL_GPIO_WritePin(PMGT_RESET_PORT, PMGT_RESET_PIN, GPIO_PIN_SET);
}

/* --- THREAD-SAFE GPIO (_S) --- */

void PIN_Set_S(const Pin_Descriptor_t* desc) {
    if (osMutexAcquire(pin_mutex, osWaitForever) == osOK) {
        HAL_GPIO_WritePin(desc->port, desc->pin, GPIO_PIN_SET);
        osMutexRelease(pin_mutex);
    }
}

void PIN_Reset_S(const Pin_Descriptor_t* desc) {
    if (osMutexAcquire(pin_mutex, osWaitForever) == osOK) {
        HAL_GPIO_WritePin(desc->port, desc->pin, GPIO_PIN_RESET);
        osMutexRelease(pin_mutex);
    }
}

void PIN_Toggle_S(const Pin_Descriptor_t* desc) {
    if (osMutexAcquire(pin_mutex, osWaitForever) == osOK) {
        HAL_GPIO_TogglePin(desc->port, desc->pin);
        osMutexRelease(pin_mutex);
    }
}

uint8_t PIN_Read_S(const Pin_Descriptor_t* desc) {
    uint8_t s = 0;
    if (osMutexAcquire(pin_mutex, osWaitForever) == osOK) {
        s = (HAL_GPIO_ReadPin(desc->port, desc->pin) == GPIO_PIN_SET);
        osMutexRelease(pin_mutex);
    }
    return s;
}

/* --- DIRECT ACCESS --- */
void PIN_Set(const Pin_Descriptor_t* desc)    { HAL_GPIO_WritePin(desc->port, desc->pin, GPIO_PIN_SET); }
void PIN_Reset(const Pin_Descriptor_t* desc)  { HAL_GPIO_WritePin(desc->port, desc->pin, GPIO_PIN_RESET); }
void PIN_Toggle(const Pin_Descriptor_t* desc) { HAL_GPIO_TogglePin(desc->port, desc->pin); }
uint8_t PIN_Read(const Pin_Descriptor_t* desc){ return (uint8_t)HAL_GPIO_ReadPin(desc->port, desc->pin); }
