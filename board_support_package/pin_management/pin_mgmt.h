/**
 * @file    pin_mgmt.h
 * @brief   Hardware Abstraction Layer for Pin Management (BV PCB Rev02)
 * @details Hybrid approach: Fast system functions and thread-safe GPIO access (_S).
 * @encoding UTF-8
 */

#ifndef __PIN_MGMT_H
#define __PIN_MGMT_H

#include "main.h"
#include "cmsis_os2.h"
#include <stdbool.h>

/* --- Pin Aliases from main.h --- */

#define PMGT_DONE_PIN         DONE_Pin
#define PMGT_DONE_PORT        DONE_GPIO_Port

#define PMGT_RESET_PIN        RESET_XS6_Pin
#define PMGT_RESET_PORT       RESET_XS6_GPIO_Port

#define PMGT_SEL_MEM_PIN      SEL_MEM_STM32_Pin
#define PMGT_SEL_MEM_PORT     SEL_MEM_STM32_GPIO_Port

#define PMGT_NSS_SPI2_PIN     SPI2_NSS_Pin
#define PMGT_NSS_SPI2_PORT    SPI2_NSS_GPIO_Port

#define PMGT_NSS_XC6_PIN      NSS_STM32_XC6_Pin
#define PMGT_NSS_XC6_PORT     NSS_STM32_XC6_GPIO_Port

#define PMGT_STLINK_GND_PIN   STLINK_GND_Pin
#define PMGT_STLINK_GND_PORT  STLINK_GND_GPIO_Port

/**
 * @brief Descriptor structure for GPIO mapping
 */
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    const char* name;
} Pin_Descriptor_t;

/* --- Pin Descriptor Exports --- */
extern const Pin_Descriptor_t pin_w_up_uart;
extern const Pin_Descriptor_t pin_fpga_done;
extern const Pin_Descriptor_t pin_reset_xs6;
extern const Pin_Descriptor_t pin_sel_mem_stm32;
extern const Pin_Descriptor_t pin_nss_spi2;
extern const Pin_Descriptor_t pin_nss_xc6;
extern const Pin_Descriptor_t pin_gpio_0;
extern const Pin_Descriptor_t pin_gpio_1;
extern const Pin_Descriptor_t pin_stlink_gnd;
extern const Pin_Descriptor_t pin_tp0;
extern const Pin_Descriptor_t pin_tp1;

osStatus_t PIN_Mgmt_Init(void);

/* API */
void     PIN_Set(const Pin_Descriptor_t* desc);
void     PIN_Reset(const Pin_Descriptor_t* desc);
void     PIN_Toggle(const Pin_Descriptor_t* desc);
uint8_t  PIN_Read(const Pin_Descriptor_t* desc);

void     PIN_Set_S(const Pin_Descriptor_t* desc);
void     PIN_Reset_S(const Pin_Descriptor_t* desc);
void     PIN_Toggle_S(const Pin_Descriptor_t* desc);
uint8_t  PIN_Read_S(const Pin_Descriptor_t* desc);

/* System Functions */
osStatus_t SPI_Bus_Acquire_For_STM32(void);
osStatus_t SPI_Bus_Release_To_FPGA(void);
osStatus_t FPGA_System_Restart(uint32_t timeout_ms);

#endif /* __PIN_MGMT_H */
