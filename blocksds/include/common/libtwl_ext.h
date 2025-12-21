/*
    Copyright (C) 2023 lifehackerhansol

    SPDX-License-Identifier: Zlib
*/

#pragma once

#include <libtwl/card/card.h>
#include <nds/ndstypes.h>

// Wrapper for reading from a cartridge.
void cardExt_ReadData(u64 command, u32 flags, void* buffer, u32 length);

// Wrapper for writing to a cartridge.
void cardExt_WriteData(u64 command, u32 flags, const void* buffer, u32 length);

// Wrapper for reading 4 bytes from a cartridge. Usually used for simple commands.
u32 cardExt_ReadData4Byte(u64 command, u32 flags);

// Wrapper for sending a command without an expected return value.
void cardExt_SendCommand(u64 command, u32 flags);

// Wrapper for writing a byte to SPI and reading back the data.
u8 cardExt_ReadWriteSpiByte(u8 data);

// Enables the SPI.
static inline void cardExt_EnableSpi(void)
{
    REG_MCCNT0 = (REG_MCCNT0 & ~(MCCNT0_MODE_MASK | MCCNT0_ROM_XFER_IRQ)) | MCCNT0_MODE_SPI | MCCNT0_SPI_HOLD_CS | MCCNT0_ENABLE;
}

// Checks if the cartridge SPI data is ready for receiving.
static inline void cardExt_WaitSpiBusy(void)
{
    while(REG_MCCNT0 & MCCNT0_SPI_BUSY);
}
