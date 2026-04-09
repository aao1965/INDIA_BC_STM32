#ifndef FRAM_RECORDER_H
#define FRAM_RECORDER_H

#include "board_support_package.h"

/* ========================================================================== */
/* FRAM RECORDER MEMORY MAP                                                   */
/* ========================================================================== */
/* * Hardware: FM22L16 (256K x 16-bit)
 * Total Capacity: 512 kB (256 kWords)
 * Max Word Address: 0x3FFFF
 * * The recorder uses the ENTIRE FRAM memory.
 * 512 kB = 524,288 Bytes = 262,144 Words (0x40000 Words).
 * * Start Address: 0x00000
 */

// Start word address of the recorder block in FRAM
#define FRAM_RECORDER_START_ADDRESS     0x00000

// End word address of the recorder block in FRAM
#define FRAM_RECORDER_END_ADDRESS       0x3FFFF

// Size of the recorder block
#define FRAM_RECORDER_SIZE_BYTES        (512 * 1024)    // 512 kB
#define FRAM_RECORDER_SIZE_WORDS        (256 * 1024)    // 256 kWords (0x40000)

// Block size for terminal transfer (Increased to 1024 for faster R/W)
#define FRAM_RECORDER_BLOCK_SIZE        1024

#endif /* FRAM_RECORDER_H */
