#ifndef FM22L16_H
#define FM22L16_H

#include "fsmc_memory.h"
#include "board_support_package.h" // Содержит t_float_uint

/* Константы памяти */
#define FM22_MAX_ADDR      0x3FFFF // 256K слов (18 бит адреса)

/* Маски секторов защиты (стр. 5 даташита) */
#define FM22_PROT_SEC_0    (1 << 0) // 00000h–07FFFh
#define FM22_PROT_SEC_1    (1 << 1) // 08000h–0FFFFh
#define FM22_PROT_SEC_2    (1 << 2) // 10000h–17FFFh
#define FM22_PROT_SEC_3    (1 << 3) // 18000h–1FFFFh
#define FM22_PROT_SEC_4    (1 << 4) // 20000h–27FFFh
#define FM22_PROT_SEC_5    (1 << 5) // 28000h–2FFFFh
#define FM22_PROT_SEC_6    (1 << 6) // 30000h–37FFFh
#define FM22_PROT_SEC_7    (1 << 7) // 38000h–3FFFFh
#define FM22_PROT_NONE     (0x00)   // Разблокировать всё
#define FM22_PROT_ALL      (0xFF)   // Заблокировать всё

/* Доступ к данным */
FSMC_Status_t fm22_write_union(uint32_t addr, t_float_uint data);
FSMC_Status_t fm22_read_union(uint32_t addr, t_float_uint *data);

/* Управление защитой записи */
FSMC_Status_t fm22_set_protection(uint8_t sector_mask);

/* Тестирование */
FSMC_Status_t fm22_test_quick(void); // Без потери данных
FSMC_Status_t fm22_test_full(void);  // С потерей данных (глубокий)

#endif
