/*
    ARDS - Datel Action Replay DS
    Card IO routines

    Copyright (C) 2023 lifehackerhansol
    Copyright (C) 2025 Edoardo Lolletti (edo9300)

    SPDX-License-Identifier: Zlib
*/

#include <nds/ndstypes.h>
#include <libtwl/card/card.h>
#include <common/libtwl_ext.h>
#include "ards.h"

void waitByLoop(int);

static bool isSdhc = false;

static void ARDS_SendNtrCommandF22(u32 param1, u8 param2) {
    cardExt_SendCommand(ARDS_CMD_F2(param1, param2), ARDS_CTRL_BASE);
}

void ARDS_SendNtrCommandF2(u8 param2);
void cardExt_EnableSpi2();
u8 ARDS_ReadSpiByte(void);
u8 cardExt_ReadWriteSpiByte2(u8);
u8 ARDS_ReadSpiByteTimeout(void);
void ARDS_CycleSpi();
u8 ARDS_SpiSendSDIOCommand2(u32 arg, u8 cmdId, int extraBytes);
u8 ARDS_SpiSendSDIOCommandR02(u32 arg, u8 cmd);

/*
static u8 ARDS_ReadSpiByte(void) {
    return cardExt_ReadWriteSpiByte(ARDS_SPI_READ_BYTE);
}
*/
// Waits for timeout, then returns resulting data.
/*
u8 ARDS_ReadSpiByteTimeout(void) {
    int timeout = ARDS_SD_CMD_TIMEOUT_LEN;
    u8 data;
    while (data = ARDS_ReadSpiByte(), data == 0xFF && --timeout > 0);
    return data;
}*/
/*
static void ARDS_CycleSpi() {
    ARDS_SendNtrCommandF2(ARDS_CMD_F2_SPI_DISABLE);
    cardExt_EnableSpi2();
	ARDS_ReadSpiByte();
    ARDS_SendNtrCommandF2(ARDS_CMD_F2_SPI_ENABLE);
    cardExt_EnableSpi2();
}*/


// Sends SDIO command to ARDS.
/*
u8 ARDS_SpiSendSDIOCommand2(u32 arg, u8 cmdId, int extraBytes)
{
	ARDS_CycleSpi();
    uint8_t cmd[6];

    // Build a SPI SD command to be sent as-is.
    cmd[0] = cmdId;
    cmd[1] = arg >> 24;
    cmd[2] = arg >> 16;
    cmd[3] = arg >> 8;
    cmd[4] = arg >> 0;

    for (int i = 0; i < sizeof(cmd); i++) cardExt_ReadWriteSpiByte2(cmd[i]);

    u8 timeout = ARDS_ReadSpiByteTimeout();

    for(int i=0; i < extraBytes; i++)
    {
        (void)ARDS_ReadSpiByte();
    }

    return timeout;
}*/
/*
u8 ARDS_SpiSendSDIOCommandR02(u32 arg, u8 cmd)
{
    return ARDS_SpiSendSDIOCommand2(arg, cmd, 0);
}*/


// Sends SDIO command to ARDS.
static u8 ARDS_SpiSendSDIOCommand(u8 cmdId, u32 arg, u8 * buffer, int len)
{
	ARDS_CycleSpi();
    uint8_t cmd[6];

    // Build a SPI SD command to be sent as-is.
    cmd[0] = 0x40 | (cmdId & 0x3f);
    cmd[1] = arg >> 24;
    cmd[2] = arg >> 16;
    cmd[3] = arg >> 8;
    cmd[4] = arg >> 0;
    // CRC in SPI mode is ignored for every command but CMD0 (hardcoded to 0x95)
    // and CMD8, hardcoded to 0x86 with the default 0x1AA argument.
    cmd[5] = (len > 0) ? 0x86 : 0x95;

    for (int i = 0; i < sizeof(cmd); i++) cardExt_ReadWriteSpiByte(cmd[i]);

    u8 timeout = ARDS_ReadSpiByteTimeout();

    const u8 * target = buffer == NULL ? NULL : (buffer + len);
    for(int i=0; i < len; i++)
    {
        u8 data = ARDS_ReadSpiByte();
        if(buffer < target)
            *buffer++ = data;
    }

    return timeout;
}

static u8 ARDS_SpiSendSDIOCommandR0(u8 cmd, u32 arg)
{
    return ARDS_SpiSendSDIOCommand(cmd, arg, NULL, 0);
}

#define ARDS_MAX_STARTUP_TRIES 5000

bool ARDS_SDInitialize(void)
{
    bool isv2 = false;
    for (int i = 0; i < 0x100; i++) {
        ARDS_SendNtrCommandF22(0x7FFFFFFF | ((i & 1) << 31), 0x00);
    }

    // Send CMD0.
    uint8_t r1 = ARDS_SpiSendSDIOCommandR0(0, 0);
    if (r1 != 0x01)  // Idle State.
    {
        // CMD 0 failed.
        return false;
    }

    uint32_t r7_answer;

    r1 = ARDS_SpiSendSDIOCommand(8, 0x1AA, (u8*)&r7_answer, 4);

    uint32_t acmd41_arg = 0;

    if (r1 == 0x1 && r7_answer == 0xAA010000) {
        isv2 = true;
        acmd41_arg |= (1 << 30);  // Set HCS bit,Supports SDHC
    }

    for (int i = 0; i < ARDS_MAX_STARTUP_TRIES; ++i) {
        // Send ACMD41.
        ARDS_SpiSendSDIOCommandR0(ARDS_SDIO_CMD55_APP_CMD, 0);
        r1 = ARDS_SpiSendSDIOCommandR0(ARDS_SDIO_ACMD41_SD_SEND_OP_COND, acmd41_arg);
        if (r1 == 0) {
            break;
        }
    }
    if (r1 != 0) return false;

    if (isv2) {
        uint32_t r2_answer;
        r1 = ARDS_SpiSendSDIOCommand(ARDS_SDIO_CMD58_READ_OCR, 0, (u8*)&r2_answer, 4);
        isSdhc = (r2_answer & 0x40) != 0;
    }
    ARDS_SpiSendSDIOCommandR0(ARDS_SDIO_CMD16_SET_BLOCK_LEN, 0x200);

    return true;
}


bool ARDS_SDReadSingleSector(u32 sector, u8 * buffer) {
    sector = isSdhc ? sector : sector << 9;

    if(ARDS_SpiSendSDIOCommandR02(sector, ARDS_SDIO_CMD17_READ_SINGLE_BLOCK | 0x40) != 0)
        return false;

    // Wait for data start token
    if(ARDS_ReadSpiByteTimeout() != ARDS_SPI_START_DATA_TOKEN)
        return false;

    for(int i=0; i < 512; i++)
        *buffer++ = ARDS_ReadSpiByte();

    (void)ARDS_ReadSpiByte();
    (void)ARDS_ReadSpiByte();

    return true;
}
// bool ARDS_SDReadMultipleSector(u32 sector, u32 num_sectors, u8 * buffer);
bool ARDS_SDReadMultipleSector2(u32 sector, u32 num_sectors, u8 * buffer) {
    sector = isSdhc ? sector : sector << 9;

    if(ARDS_SpiSendSDIOCommandR02(sector, ARDS_SDIO_CMD18_READ_MULTIPLE_BLOCK | 0x40) != 0)
        return false;

    for(int i=0; i < num_sectors; i++)
    {
        // Wait for data start token
        if(ARDS_ReadSpiByteTimeout() != ARDS_SPI_START_DATA_TOKEN)
            return false;

        for(int j=0; j < 512; j++)
            *buffer++ = ARDS_ReadSpiByte();

        (void)ARDS_ReadSpiByte();
        (void)ARDS_ReadSpiByte();
    }

    // this message returns 1 byte of response, but it needs up to 8 bytes
    // to be polled before we get the busy bytes
    ARDS_SpiSendSDIOCommand2(0, ARDS_SDIO_CMD12_STOP_TRANSMISSION | 0x40, 7);

    // Wait for card to finish
    int timeout = ARDS_SD_CMD_TIMEOUT_LEN;
    while (ARDS_ReadSpiByte() == 0 && --timeout > 0);

    return timeout != 0;
}

bool ARDS_SDWriteSingleSector(u32 sector, const u8 * buffer)
{
    sector = isSdhc ? sector : sector << 9;

    // this message needs 1 byte of extra clock before it starts waiting for the start token
    if(ARDS_SpiSendSDIOCommand2(sector, ARDS_SDIO_CMD24_WRITE_SINGLE_BLOCK | 0x40, 1) != 0)
        return false;

    // Send start token
    cardExt_ReadWriteSpiByte(ARDS_SPI_START_DATA_TOKEN);

    // Send data
    for (int i = 0; i < 512; i++)
        cardExt_ReadWriteSpiByte(*buffer++);

    // Send fake CRC
    ARDS_ReadSpiByte();
    ARDS_ReadSpiByte();

    // Get data response
    if((ARDS_ReadSpiByte() & 0x0F) != ARDS_SD_WRITE_OK)
        return false;

    // Wait for card to write data
    int timeout = ARDS_SD_WRITE_TIMEOUT_LEN;
    while (ARDS_ReadSpiByte() == 0 && --timeout > 0);

    return timeout != 0;
}
// bool ARDS_SDWriteMultipleSector(u32 sector, u32 num_sectors, const u8 * buffer);
bool ARDS_SDWriteMultipleSector2(u32 sector, u32 num_sectors, const u8 * buffer) {
    sector = isSdhc ? sector : sector << 9;

    // this message needs 1 byte of extra clock before it starts waiting for the start token
    if(ARDS_SpiSendSDIOCommand2(sector, ARDS_SDIO_CMD25_WRITE_MULTIPLE_BLOCK | 0x40, 1) != 0)
        return false;

    for(int i=0; i < num_sectors; i++)
    {
        // Send start token
        cardExt_ReadWriteSpiByte(ARDS_SPI_MULTI_BLOCK_WRITE_TOKEN);

        // Send data
        for (int j = 0; j < 512; j++)
            cardExt_ReadWriteSpiByte(*buffer++);

        // Send fake CRC
        ARDS_ReadSpiByte();
        ARDS_ReadSpiByte();

        // Get data response
        if((ARDS_ReadSpiByte() & 0x0F) != ARDS_SD_WRITE_OK)
            return false;

        // Wait for card to write data
        int timeout = ARDS_SD_WRITE_TIMEOUT_LEN;
        while (ARDS_ReadSpiByte() == 0 && --timeout > 0);
        if(!timeout)
            return false;
    }

    // send stop token
    cardExt_ReadWriteSpiByte(ARDS_SPI_END_MULTI_BLOCK_WRITE);
    // send 1 byte clock
    ARDS_ReadSpiByte();

    int timeout = ARDS_SD_WRITE_TIMEOUT_LEN;
    while (ARDS_ReadSpiByte() == 0 && --timeout > 0);

    return timeout != 0;
}
