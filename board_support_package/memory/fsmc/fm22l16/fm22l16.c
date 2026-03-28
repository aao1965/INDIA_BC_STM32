#include "fm22l16.h"

/* Запись 32-бит данных (2 слова по 16 бит) */
FSMC_Status_t fm22_write_union(uint32_t addr, t_float_uint data) {
    return fsmc_write_buffer(FSMC_DEV_FRAM, addr, data.u16, 2);
}

/* Чтение 32-бит данных */
FSMC_Status_t fm22_read_union(uint32_t addr, t_float_uint *data) {
    if (!data) return FSMC_ERROR;
    return fsmc_read_buffer(FSMC_DEV_FRAM, addr, data->u16, 2);
}

/* Программная блокировка/разблокировка секторов (Sequence Detector) */
FSMC_Status_t fm22_set_protection(uint8_t sector_mask) {
    uint16_t dummy;
    if (fsmc_lock() != FSMC_OK) return FSMC_TIMEOUT;

    /* 6 специфических чтений */
    dummy = fsmc_read_fast(FSMC_DEV_FRAM, 0x24555);
    dummy = fsmc_read_fast(FSMC_DEV_FRAM, 0x3AAAA);
    dummy = fsmc_read_fast(FSMC_DEV_FRAM, 0x02333);
    dummy = fsmc_read_fast(FSMC_DEV_FRAM, 0x1CCCC);
    dummy = fsmc_read_fast(FSMC_DEV_FRAM, 0x000FF);
    dummy = fsmc_read_fast(FSMC_DEV_FRAM, 0x3EF00);
    (void)dummy;

    /* 3 специфических записи */
    fsmc_write_fast(FSMC_DEV_FRAM, 0x3AAAA, (uint16_t)sector_mask);
    fsmc_write_fast(FSMC_DEV_FRAM, 0x1CCCC, (uint16_t)~sector_mask);
    fsmc_write_fast(FSMC_DEV_FRAM, 0x0FF00, 0x0000);

    /* Завершающее чтение */
    fsmc_read_fast(FSMC_DEV_FRAM, 0x00000);

    fsmc_unlock();
    return FSMC_OK;
}

/* Быстрый тест 3-х точек (начало, середина, конец) с сохранением данных */
FSMC_Status_t fm22_test_quick(void) {
    uint32_t pts[] = {0, FM22_MAX_ADDR / 2, FM22_MAX_ADDR};
    uint16_t orig, pattern = 0xAA55;
    FSMC_Status_t status = FSMC_OK;

    if (fsmc_lock() != FSMC_OK) return FSMC_TIMEOUT;

    for (uint8_t i = 0; i < 3; i++) {
        orig = fsmc_read_fast(FSMC_DEV_FRAM, pts[i]);

        fsmc_write_fast(FSMC_DEV_FRAM, pts[i], pattern);
        if (fsmc_read_fast(FSMC_DEV_FRAM, pts[i]) != pattern) status = FSMC_ERROR;

        fsmc_write_fast(FSMC_DEV_FRAM, pts[i], (uint16_t)~pattern);
        if (fsmc_read_fast(FSMC_DEV_FRAM, pts[i]) != (uint16_t)~pattern) status = FSMC_ERROR;

        fsmc_write_fast(FSMC_DEV_FRAM, pts[i], orig); // Восстановление
        if (status != FSMC_OK) break;
    }

    fsmc_unlock();
    return status;
}

/* Полный тест всей памяти (проверка уникальности адресов и каждого бита) */
FSMC_Status_t fm22_test_full(void) {
    if (fsmc_lock() != FSMC_OK) return FSMC_TIMEOUT;

    /* Запись собственного адреса в каждую ячейку */
    for (uint32_t i = 0; i <= FM22_MAX_ADDR; i++) {
        fsmc_write_fast(FSMC_DEV_FRAM, i, (uint16_t)i);
    }
    for (uint32_t i = 0; i <= FM22_MAX_ADDR; i++) {
        if (fsmc_read_fast(FSMC_DEV_FRAM, i) != (uint16_t)i) {
            fsmc_unlock(); return FSMC_ERROR;
        }
    }

    /* Запись инверсного адреса */
    for (uint32_t i = 0; i <= FM22_MAX_ADDR; i++) {
        fsmc_write_fast(FSMC_DEV_FRAM, i, (uint16_t)~i);
    }
    for (uint32_t i = 0; i <= FM22_MAX_ADDR; i++) {
        if (fsmc_read_fast(FSMC_DEV_FRAM, i) != (uint16_t)~i) {
            fsmc_unlock(); return FSMC_ERROR;
        }
    }

    fsmc_unlock();
    return FSMC_OK;
}
