/*
    EZ-Flash V
    Card IO routines

    Copyright (C) 2007 Michael Chisholm (Chishm)
    Copyright (C) 2007 SaTa
    Copyright (C) 2023 lifehackerhansol

    SPDX-License-Identifier: Zlib
*/

#include <common/libtwl_ext.h>
#include <common/sdio.h>
#include <libcart/sdcrc16.h>
#include <libtwl/card/card.h>
#include <nds/ndstypes.h>
#include <string.h>
#include "ez5h.h"

static u32 isSDHC = 0;

u32 EZ5H_SendCommand(const u64 command) {
    return cardExt_RomReadData4Byte(command, EZ5H_CTRL_READ_4B);
}

static bool EZ5H_SDSendSDIOCommand(u8 cmd, u32 parameter, u8* buffer, int size) {
    u32 data;
    u8* u8_data = (u8*)&data;
    int timeout = 99;

    // Either it's no response, 48 bits, or 136 bits.
    // If this isn't the case, then this function doesn't work.
    if (size != 0 && size != 6 && size != 17) return false;

    EZ5H_SendCommand(EZ5H_CMD_SDMC_SDIO(cmd, parameter));

    // R0 has no response.
    if (size == 0) return true;

    // Sends response in byte-swapped u32, with the starting marker
    // Search for starting marker, with a timeout
    do {
        data = EZ5H_SendCommand(EZ5H_CMD_SDMC_SEND_CLK(1));
        timeout--;
        if (!timeout) return false;
    } while (data & 0xFF);

    // Starting marker found. Start reading response
    if (buffer != NULL) {
        buffer[0] = u8_data[1];
        buffer[1] = u8_data[2];
        buffer[2] = u8_data[3];
    }

    // Read remaining data
    data = EZ5H_SendCommand(EZ5H_CMD_SDMC_SEND_CLK(2));
    if (buffer != NULL) {
        buffer[3] = u8_data[0];
        buffer[4] = u8_data[1];
        buffer[5] = u8_data[2];
        // if we're pulling an R1 response, then we have read all of our data here
        // otherwise keep going
        if (size != 6) return true;
        buffer[6] = u8_data[3];
    } else if (size == 6)
        return true;

    data = EZ5H_SendCommand(EZ5H_CMD_SDMC_SEND_CLK(3));
    if (buffer != NULL) {
        buffer[7] = u8_data[0];
        buffer[8] = u8_data[1];
        buffer[9] = u8_data[2];
        buffer[10] = u8_data[3];
    }
    data = EZ5H_SendCommand(EZ5H_CMD_SDMC_SEND_CLK(4));
    if (buffer != NULL) {
        buffer[11] = u8_data[0];
        buffer[12] = u8_data[1];
        buffer[13] = u8_data[2];
        buffer[14] = u8_data[3];
    }
    data = EZ5H_SendCommand(EZ5H_CMD_SDMC_SEND_CLK(5));
    if (buffer != NULL) {
        buffer[15] = u8_data[0];
        buffer[16] = u8_data[1];
    }

    return true;
}

u32 EZ5H_SRAMReadData(u32 address) {
    return EZ5H_SendCommand(EZ5H_CMD_SRAM_READ_DATA(address));
}

bool EZ5H_SDInitialize(void) {
    u8 response[17] = {};
    register bool isSD20 = false;

    // Does this flush something?
    // iSmart does this loop
    for (int i = 0; i < 128; i++)
        cardExt_RomReadData(EZ5H_CMD_SDMC_READ_DATA, EZ5H_CTRL_READ_512B, NULL, 0);
    EZ5H_SDSendSDIOCommand(SDIO_CMD0_GO_IDLE_STATE, 0, NULL, 0);
    // it does it twice
    for (int i = 0; i < 128; i++)
        cardExt_RomReadData(EZ5H_CMD_SDMC_READ_DATA, EZ5H_CTRL_READ_512B, NULL, 0);

    // CMD8 SDHC init
    if (EZ5H_SDSendSDIOCommand(SDIO_CMD8_SEND_IF_COND, 0x1AA, response, 6))
        if (response[3] == 1 && response[4] == 0xAA) isSD20 = true;

    do {
        EZ5H_SDSendSDIOCommand(SDIO_CMD55_APP_CMD, 0, NULL, 6);
        u32 parameter = 0x00800000;
        if (isSD20) parameter |= BIT(30);
        EZ5H_SDSendSDIOCommand(SDIO_ACMD41_SD_SEND_OP_COND, parameter, response, 6);
    } while (!(response[1] & 0x80));
    isSDHC = response[1] & 0x40 ? 1 : 0;

    EZ5H_SDSendSDIOCommand(SDIO_CMD2_ALL_SEND_CID, 0, NULL, 17);
    do {
        EZ5H_SDSendSDIOCommand(SDIO_CMD3_SEND_RELATIVE_ADDR, 0, response, 6);
    } while ((response[3] & 0x1E) != 6);  // is standby

    u32 sdio_rca = (response[1] << 8) + response[2];

    EZ5H_SDSendSDIOCommand(SDIO_CMD9_SEND_CSD, (sdio_rca << 16), NULL, 17);
    EZ5H_SDSendSDIOCommand(SDIO_CMD7_SELECT_CARD, (sdio_rca << 16), NULL, 6);
    EZ5H_SDSendSDIOCommand(SDIO_CMD55_APP_CMD, (sdio_rca << 16), NULL, 6);
    EZ5H_SDSendSDIOCommand(SDIO_ACMD6_SET_BUS_WIDTH, 2, NULL, 6);
    EZ5H_SDSendSDIOCommand(SDIO_CMD16_SET_BLOCK_LEN, 512, NULL, 6);
    return true;
}

// Sends a clock, reads data from response index if available
static inline u64 EZ5H_CMD_SDMC_SEND_CLK2(void) {
    return EZ5H_CMD_SDMC_PARAM_CARD(1, 0, 0);
}

void cardExt_RomSendCommand2(u64 command, u32 flags) {
    card_romSetCmd(command);
    card_romStartXfer(flags | MCCNT1_LEN_0, false);
    card_romWaitBusy();
}

static void EZ5H_SendCommand2(const u64 command) {
    cardExt_RomSendCommand2(command, EZ5H_CTRL_READ_0);
}

bool EZ5H_SDSendSDIOCommand2(u8 cmd, u32 parameter);
bool EZ5H_SDSendSDIOCommand22(u8 cmd, u32 parameter) {
    int timeout = 0x10000;

    EZ5H_SendCommand(EZ5H_CMD_SDMC_SDIO(cmd, parameter));

    // Sends response in byte-swapped u32, with the starting marker
    // Search for starting marker, with a timeout
    while (EZ5H_SendCommand(EZ5H_CMD_SDMC_SEND_CLK2()) & 0xFF) {
        timeout--;
        if (timeout == 0) return false;
    }

    return true;
}

bool EZ5H_SDReadSector2(u32 sector, void* buffer) {
    if (!isSDHC) sector <<= 9;

    if (!EZ5H_SDSendSDIOCommand2(17, sector)) return false;

    cardExt_RomReadData(EZ5H_CMD_SDMC_READ_DATA, EZ5H_CTRL_READ_512B, buffer, 128);
    return true;
}

void cardExt_RomSendWriteDataog(const u8* data)
 {
	u64 command = EZ5H_CMD_SDMC_WRITE_DATA(data);
    card_romWaitBusy();
    card_romSetCmd(command);
    card_romStartXfer(EZ5H_CTRL_READ_0 | MCCNT1_LEN_0, false);
}

static uint64_t inline calSingleCRC16(uint64_t crc, uint32_t data_in){
	// Shift out 8 bits for each line
	uint32_t data_out = crc >> 32;
	crc <<= 32;

	// XOR outgoing data to itself with 4 bit delay
	data_out ^= (data_out >> 16);

	// XOR incoming data to outgoing data with 4 bit delay
	data_out ^= (data_in >> 16);

	// XOR outgoing and incoming data to accumulator at each tap
	uint64_t xorred = data_out ^ data_in;
	crc ^= xorred;
	crc ^= xorred << (5 * 4);
	crc ^= xorred << (12 * 4);
	return crc;
}
uint64_t sdio_crc16_4bit_checksum(void* dataBuf)
{
	uint32_t num_words = 512 / sizeof(uint32_t);
	uint64_t crc = 0;
    uint32_t* data = (uint32_t*)(dataBuf);
    uint32_t* end = data + num_words;
    while (data < end)
    {
        uint32_t data_in = __builtin_bswap32(*data++);
        crc = calSingleCRC16(crc, data_in);
    }

	return __builtin_bswap64(crc);
}

inline u32 cardExt_RomReadData4ByteCustom(u64 command, u32 flags) {
    card_romSetCmd(command);
    card_romStartXfer(EZ5H_CTRL_READ_4B | MCCNT1_LEN_4, false);
    card_romWaitDataReady();
    return card_romGetData();
}
static inline void cardExt_RomSendWriteData(u16 data16) {
	u8* data = (u8*)&data16;
	u64 command = 0xF6B8;
	command |= ((u64)(data[0]) << 24);
	command |= ((u64)(data[0] >> 4) << 16);
	command |= ((u64)(data[1]) << 40);
	command |= ((u64)(data[1] >> 4) << 32);
   *(vu64*)&REG_MCCMD0 = command;
    card_romWaitBusy();
    card_romStartXfer(EZ5H_CTRL_READ_0 | MCCNT1_LEN_0, false);
}
static void [[gnu::noinline]] cardExt_RomSendWriteData2(u8* data) {
	volatile u64 a = EZ5H_CMD_SDMC_WRITE_DATA(data);
	// u64 command = __builtin_bswap64(a);
	// command |= ((u64)(data[0]) << 24);
	// command |= ((u64)(data[0] >> 4) << 16);
	// command |= ((u64)(data[1]) << 40);
	// command |= ((u64)(data[1] >> 4) << 32);
   // *(vu64*)&REG_MCCMD0 = command;
   card_romSetCmd(a);
    card_romStartXfer(EZ5H_CTRL_READ_0 | MCCNT1_LEN_0, false);
    card_romWaitBusy();
}

void cardExt_RomSendWriteDatacool(const u8* datab)
 {
    u8* base = (u8*)&REG_MCCMD0;
    *((u16*)base) = __builtin_bswap64(EZ5H_CMD_SDMC | 0x00F6000000000000ull);
	
	u16 data;
	memcpy(&data, datab, 2);

	// command |= ((u64)(data[0]) << 24);
	// command |= ((u64)(data[0] >> 4) << 16);
	// command |= ((u64)(data[1]) << 40);
	// command |= ((u64)(data[1] >> 4) << 32);

    *(base + 3) = data | 0xF0;
    data >>= 4;
    *(base + 2) = data | 0xF0;
    data >>= 4;
    *(base + 5) = data | 0xF0;
    data >>= 4;
    *(base + 4) = data | 0xF0;
    card_romWaitBusy();
    card_romStartXfer(EZ5H_CTRL_READ_0 | MCCNT1_LEN_0, false);
}


bool EZ5H_SDWriteSector(u32 sector, const u8* buffer) {
    if (!isSDHC) sector <<= 9;

	// u16 crc16buff[4];
	// {
		u64 crc16 = sdio_crc16_4bit_checksum(buffer);
		// __builtin_memcpy(crc16buff, &crc16, 8);
	// }
	// u16* crc16buff = (u16*)&crc16;

    // CMD24
    if (!EZ5H_SDSendSDIOCommand2(24 | 0x40, sector)) return false;

    // This command needs an additional clock before sending data.
    (void)EZ5H_SendCommand(EZ5H_CMD_SDMC_SEND_CLK2());

    // Send data start marker.
    u16 start_marker = 0xF0FF;
	
    // u8 start_marker[2] = {0xFF, 0xF0};
	cardExt_RomSendWriteData(start_marker);

    // EZ5H_SendCommand2(EZ5H_CMD_SDMC_WRITE_DATA(start_marker));
#if 0
    cardExt_RomSendWriteDatacool(start_marker);
#endif

#if 1
    // Write data.
    for (u32 i = 0; i < 512; i += 2) {
        cardExt_RomSendWriteData2((buffer + i));
    }
    // Write CRC data.
    for (u32 i = 0; i < 8; i += 1) {
        cardExt_RomSendWriteData2((((u8*)&crc16) + i));
    }
#else
    // Write data.
    for (u32 i = 0; i < 512; i += 2) {
        cardExt_RomSendCommand(EZ5H_CMD_SDMC_WRITE_DATA(buffer + i), EZ5H_CTRL_READ_0);
    }
    // Write CRC data.
    for (u32 i = 0; i < 8; i += 2) {
        cardExt_RomSendCommand(EZ5H_CMD_SDMC_WRITE_DATA(((u8*)&crc16) + i), EZ5H_CTRL_READ_0);
    }
#endif
#if 0
    // Write data.
    for (u32 i = 0; i < 512; i += 2) {
        cardExt_RomSendWriteDatacool((buffer + i));
    }
    // Write CRC data.
    for (u32 i = 0; i < 8; i += 1) {
        cardExt_RomSendWriteDatacool(((u8*)&crc16) + i);
    }
    card_romWaitBusy();
#endif

    // Wait until CRC starts
    while (EZ5H_SendCommand(EZ5H_CMD_SDMC_SEND_CRC_STATUS) & 0x1);

    // Read CRC status
    while ((EZ5H_SendCommand(EZ5H_CMD_SDMC_SEND_CRC_STATUS) & 0x1) != 0x1);

    // Wait until card ready
    while (EZ5H_SendCommand(EZ5H_CMD_SDMC_SEND_CLK2()) & 0xFF);
    return true;
}
