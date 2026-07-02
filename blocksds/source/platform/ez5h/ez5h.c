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

#include "ez5h.h"

static u32 isSDHC = 0;

static u32 EZ5H_SendCommand(const u64 command) {
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

extern uint32_t ez5h_readSector_addr;
extern uint32_t ez5h_writeSector_addr;
extern uint16_t ez5h_sdhc_read_label;
extern uint16_t ez5h_sdhc_write_label;

extern u32 ez5h_doSDOperation_sendSDIOCommand;
extern u32 ez5h_writeSector_sendCommand;

extern u32 ez5h_writeSector_sdio4BitCrc16;

extern u32 ez5h_writeSector_doSDOperation;
extern u32 ez5h_readSector_doSDOperation;

void ez5h_sendSDIOCommand();
void ez5h_sendCommand();
void ez5h_sendWriteDataRomCommand(const u8* datab);
void ez5h_sendWriteDataRomCommandShort(u16 data);
void ez5h_sdio4BitCrc16();
void ez5h_doSDOperation();

bool EZ5H_SDInitialize(void) {
	ez5h_writeSector_addr = (unsigned)&ez5h_writeSector;

	ez5h_doSDOperation_sendSDIOCommand = (unsigned)&ez5h_sendSDIOCommand;
	ez5h_writeSector_sendCommand = (unsigned)&ez5h_sendCommand;

	ez5h_writeSector_sdio4BitCrc16 = (unsigned)&ez5h_sdio4BitCrc16;

	ez5h_writeSector_doSDOperation = (unsigned)&ez5h_doSDOperation;
	ez5h_readSector_doSDOperation = (unsigned)&ez5h_doSDOperation;


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
	const uint16_t non_sdhc_opcode = 0x0241; //lsls r1,r0,#9
	const uint16_t sdhc_opcode = 0x0001; //movs r1,r0
	if(isSDHC) {
		ez5h_sdhc_read_label = sdhc_opcode;
		ez5h_sdhc_write_label = sdhc_opcode;
	} else {
		ez5h_sdhc_read_label = non_sdhc_opcode;
		ez5h_sdhc_write_label = non_sdhc_opcode;
	}
    return true;
}
