// SPDX-License-Identifier: CC0-1.0
//
// SPDX-FileContributor: Antonio Niño Díaz, 2023

#include <iointerface.h>
#include <stdbool.h>
#include <stdint.h>

#include "ards.h"

#define BYTES_PER_READ 512

// Initialize the driver. Returns true on success.
bool ARDS_Startup(void) {
    return ARDS_SDInitialize();
}

// Returns true if a card is present and initialized.
bool ARDS_IsInserted(void) {
    return true;
}

// Clear error flags from the card. Returns true on success.
bool ARDS_ClearStatus(void) {
    return true;
}

// Reads 512 byte sectors into a buffer that may be unaligned. Returns true on
// success.
bool ARDS_ReadSectors(uint32_t sector, uint32_t num_sectors, void* buffer) {
    // if (num_sectors == 1)
        // ARDS_SDReadSingleSector(sector, buffer);
    // else
        return ARDS_SDReadMultipleSector(sector, buffer, num_sectors);
    // return true;
}

// Writes 512 byte sectors from a buffer that may be unaligned. Returns true on
// success.
bool ARDS_WriteSectors(uint32_t sector, uint32_t num_sectors, const void* buffer) {
    // if (num_sectors == 1)
        // ARDS_SDWriteSingleSector(sector, buffer);
    // else
        return ARDS_SDWriteMultipleSector(sector, buffer, num_sectors);
    // return true;
}

// Shutdowns the card. This may never be called.
bool ARDS_Shutdown(void) {
    return true;
}

#ifdef PLATFORM_ards

disc_interface_t ioInterface = {.startup = ARDS_Startup,
                                .is_inserted = ARDS_IsInserted,
                                .read_sectors = ARDS_ReadSectors,
                                .write_sectors = ARDS_WriteSectors,
                                .clear_status = ARDS_ClearStatus,
                                .shutdown = ARDS_Shutdown};

#endif
