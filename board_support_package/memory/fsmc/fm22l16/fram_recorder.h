#ifndef FRAM_RECORDER_H
#define FRAM_RECORDER_H

#include "board_support_package.h"

/* ========================================================================== */
/* FRAM RECORDER MEMORY MAP                              */
/* ========================================================================== */
/* * Hardware: FM22L16 (256K x 16-bit)
 * Total Capacity: 512 kB (256 kWords)
 * Max Word Address: 0x3FFFF
 * * The recorder uses the last 64 kB block of the FRAM memory.
 * 64 kB = 65,536 Bytes = 32,768 Words (0x8000 Words).
 * * Start Address calculation: 0x3FFFF - 0x8000 + 1 = 0x38000
 */

// Start word address of the recorder block in FRAM
#define FRAM_RECORDER_START_ADDRESS     0x38000

// End word address of the recorder block in FRAM
#define FRAM_RECORDER_END_ADDRESS       0x3FFFF

// Size of the recorder block
#define FRAM_RECORDER_SIZE_BYTES        (64 * 1024)     // 64 kB
#define FRAM_RECORDER_SIZE_WORDS        (32 * 1024)     // 32 kWords (0x8000)

// Block size for terminal transfer (Increased to 1024 for faster R/W)
#define FRAM_RECORDER_BLOCK_SIZE        1024


#endif /* FRAM_RECORDER_H */
