#include "application.h"
#include <string.h>

/*
 * Чтение из внутренней Flash-памяти
 * Выполняется с максимальной скоростью системной шины
 */
void eeprom_memory_read_buffer(uint8_t *dst, const uint8_t *src, uint32_t len) {
    memcpy(dst, src, len);
}

/*
 * Стирание одного сектора Flash-памяти по его номеру (например, FLASH_SECTOR_5).
 * Внимание: Процесс стирания 128КБ сектора может занимать до 2 секунд.
 * Во время стирания выполнение кода из Flash аппаратно приостанавливается.
 */
bool eeprom_memory_erase(uint32_t sector) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    bool result = true;

    // Настройка параметров стирания
    EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3; // Требует питания 2.7-3.6V для быстрой работы
    EraseInitStruct.Sector        = sector;
    EraseInitStruct.NbSectors     = 1;

    // Снимаем защиту с контроллера Flash
    if (HAL_FLASH_Unlock() != HAL_OK) return false;

    // Очищаем возможные предыдущие ошибки
    CLEAR_FLAG;

    // Запускаем процесс стирания сектора
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        result = false;
    }

    // Возвращаем защиту
    HAL_FLASH_Lock();
    
    return result;
}

/*
 * Быстрая запись 32-битных слов во внутреннюю Flash-память.
 * Используется FLASH_TYPEPROGRAM_WORD, что в 4 раза быстрее побайтовой записи.
 * * ВАЖНО: 
 * 1. Адрес `addr` должен быть кратен 4 (выровнен).
 * 2. Область памяти перед записью должна быть стерта (содержать 0xFFFFFFFF).
 */
bool eeprom_memory_write_words(const uint32_t *p_data, uint32_t addr, uint32_t words_count) {
    bool result = true;

    // Проверка выравнивания адреса 
    if (addr % 4 != 0) return false;

    if (HAL_FLASH_Unlock() != HAL_OK) return false;

    CLEAR_FLAG;

    // Запись данных циклом по 4 байта
    for (uint32_t i = 0; i < words_count; ++i) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, p_data[i]) == HAL_OK) {
            addr += 4; // Сдвигаем адрес на 4 байта вперед
        } else {
            result = false;
            break; // Прерываем запись при первой же ошибке
        }
    }

    HAL_FLASH_Lock();
    
    return result;
}



bool check_application(void) {
    fw_header_t *fw_info = NULL;
    uint32_t *search_ptr = (uint32_t *)APPLICATION_ADDRESS;

    // 1. Ищем магическое слово (сканируем первые 1024 байта / 256 слов)
    for (int i = 0; i < 256; i++) {
        if (search_ptr[i] == 0x55AA55AA) {
            fw_info = (fw_header_t *)&search_ptr[i];
            break;
        }
    }

    // 2. Базовые проверки на отсутствие прошивки или "непропатченность"
    if (fw_info == NULL || fw_info->fw_size == 0xFFFFFFFF) {
        return false;
    }

    // 3. Подготовка к расчету CRC
    uint32_t *payload_ptr = (uint32_t *)(fw_info + 1);
    uint32_t header_offset_bytes = (uint32_t)payload_ptr - APPLICATION_ADDRESS;

    // Защита: если размер указан криво (меньше самого заголовка),
    // чтобы не уйти в HardFault при делении и расчете:
    if (fw_info->fw_size <= header_offset_bytes) {
        return false;
    }

    uint32_t payload_bytes = fw_info->fw_size - header_offset_bytes;
    uint32_t payload_words = payload_bytes / 4; // Переводим в 32-битные слова

    // 4. Считаем аппаратный CRC
    uint32_t calc_crc = HAL_CRC_Calculate(&hcrc, payload_ptr, payload_words);

    // 5. Возвращаем результат сравнения (true/false)
    return (calc_crc == fw_info->crc32);
}

