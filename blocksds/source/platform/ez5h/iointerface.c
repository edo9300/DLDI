// SPDX-License-Identifier: CC0-1.0
//
// SPDX-FileContributor: Antonio Niño Díaz, 2023

#include <iointerface.h>
#include <stdbool.h>
#include <stdint.h>

#include "ez5h.h"

#define BYTES_PER_READ 512

int ez5h_readMultipleSector(uint32_t sector, void* buffer, uint32_t num_sectors);
int ez5h_writeMultipleSector(uint32_t sector, const void* buffer, uint32_t num_sectors);

// Initialize the driver. Returns true on success.
bool EZ5H_Startup(void) {
    return EZ5H_SDInitialize();
}

// Returns true if a card is present and initialized.
bool EZ5H_IsInserted(void) {
    return true;
}

// Clear error flags from the card. Returns true on success.
bool EZ5H_ClearStatus(void) {
    return true;
}

[[gnu::noinline]] static bool doOperation(uint32_t sector, uint32_t num_sectors, void* buffer, bool(*operation)(u32 sector, void* buffer)){
    for (int i = 0; i < num_sectors; i++) {
        bool result = operation(sector, buffer);
        if (!result) return false;
        sector++;
        buffer = (u8*)buffer + 0x200;
    }
    return true;
}

// Reads 512 byte sectors into a buffer that may be unaligned. Returns true on
// success.
bool EZ5H_ReadSectors2(uint32_t sector, uint32_t num_sectors, void* buffer) {
	return doOperation(sector, num_sectors, buffer, ez5h_readSector);
    // for (int i = 0; i < num_sectors; i++) {
        // bool result = ez5h_readSector(sector, buffer);
        // if (!result) return false;
        // sector++;
        // buffer = (u8*)buffer + 0x200;
    // }
    // return true;
}

// Writes 512 byte sectors from a buffer that may be unaligned. Returns true on
// success.
bool EZ5H_WriteSectorsa(uint32_t sector, uint32_t num_sectors, const void* buffer) {
	return doOperation(sector, num_sectors, buffer, ez5h_writeSector);
    // for (int i = 0; i < num_sectors; i++) {
        // bool result = ez5h_writeSector(sector, buffer);
        // if (!result) return false;
        // sector++;
        // buffer = (u8*)buffer + 0x200;
    // }
    // return true;
}

// Shutdowns the card. This may never be called.
bool EZ5H_Shutdown(void) {
    return true;
}

static bool EZ5H_WriteSectors(uint32_t sector, uint32_t num_sectors, const void* buffer) {
	return ez5h_writeMultipleSector(sector, buffer, num_sectors);
}
bool EZ5H_ReadSectors(uint32_t sector, uint32_t num_sectors, void* buffer) {
	return ez5h_readMultipleSector(sector, buffer, num_sectors);
}

#ifdef PLATFORM_ez5h

disc_interface_t ioInterface = {.startup = EZ5H_Startup,
                                .is_inserted = EZ5H_IsInserted,
                                .read_sectors = EZ5H_ReadSectors,
                                .write_sectors = EZ5H_WriteSectors,
                                .clear_status = EZ5H_ClearStatus,
                                .shutdown = EZ5H_Shutdown};

#endif
