#ifndef FPGA_H
#define FPGA_H

#include "fsmc_memory.h"
#include "board_support_package.h"

/* 10-битный адрес (1024 регистра) */
#define FPGA_MAX_ADDR    1023 

/* Функции доступа к регистрам */
FSMC_Status_t fpga_write(uint16_t addr, uint16_t data);
FSMC_Status_t fpga_read(uint16_t addr, uint16_t *data);

/* Работа с буферами */
FSMC_Status_t fpga_write_buf(uint16_t addr, const uint16_t *p_data, uint32_t len);
FSMC_Status_t fpga_read_buf(uint16_t addr, uint16_t *p_data, uint32_t len);

/* Битовые операции (RMW) */
FSMC_Status_t fpga_set_bit(uint16_t addr, uint16_t mask);
FSMC_Status_t fpga_clr_bit(uint16_t addr, uint16_t mask);
FSMC_Status_t fpga_tgl_bit(uint16_t addr, uint16_t mask);
uint8_t       fpga_test_bit(uint16_t addr, uint16_t mask);

/* Проверка связи  */
FSMC_Status_t fpga_test_link(void);

#endif /* FPGA_H */
