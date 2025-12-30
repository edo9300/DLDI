/*
	io_cf_common.c based on

	compact_flash.c
	By chishm (Michael Chisholm)

	Common hardware routines for using a compact flash card. This is not reentrant 
	and does not do range checking on the supplied addresses. This is designed to 
	be as fast as possible.

	CF routines modified with help from Darkfader

 Copyright (c) 2006 Michael "Chishm" Chisholm
	
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "io_cf_common.h"
#include "io_cf_regs.h"

#define BYTES_PER_READ 512

bool _CF_isInserted(void) {
    CF_REG_STATUS = CF_STS_INSERTED;
    return ((CF_REG_STATUS & 0xff) == CF_STS_INSERTED);
}

bool _CF_clearStatus(void) {
	for (int i = 0; i < CF_CARD_TIMEOUT; i++)
	{
		if ((CF_REG_COMMAND & CF_STS_BUSY) == 0)
			break;
	}

	for (int i = 0; i < CF_CARD_TIMEOUT; i++)
	{
		if ((CF_REG_STATUS & CF_STS_INSERTED) != 0)
		{
			return true;
		}
	}

    return false;
}

static bool waitReady()
{
	for (int i = 0; i < CF_CARD_TIMEOUT; i++)
	{
		if ((CF_REG_STATUS & 0xff) == CF_STS_READY)
		{
			return true;
		}
	}
	return false;
}

static bool CF_PerformTransferSectors(u8 command, u32 sector, void* dataAddr, u32 numSectors)
{
    if (!_CF_clearStatus()) return false;
	
	u32* src = (command == CF_CMD_READ) ? (u32*)&CF_REG_DATA : (u32*)dataAddr;
	u32* dst = (command == CF_CMD_READ) ? (u32*)dataAddr : (u32*)&CF_REG_DATA;

    CF_REG_SECTOR_COUNT = (numSectors < 256 ? numSectors : 0);

    CF_REG_LBA1 = sector & 0xFF;
    CF_REG_LBA2 = (sector >> 8) & 0xFF;
    CF_REG_LBA3 = (sector >> 16) & 0xFF;
    CF_REG_LBA4 = ((sector >> 24) & 0x0F) | CF_CMD_LBA;

    CF_REG_COMMAND = command;

    while (numSectors--) {
        if (!waitReady()) return false;
		for(int i = 0; i < (BYTES_PER_READ / sizeof(u32)); ++i)
		{
			*dst++ = *src++;
		}
    }

    return true;
}

static bool CF_PerformTransfer(u8 command, u32 sector, u32 numSectors, void* buffer) {
    while (numSectors > 0) {
        u32 sector_count;
        sector_count = numSectors >= 256 ? 256 : numSectors;
        if (!CF_PerformTransferSectors(command, sector, buffer, sector_count)) return false;
        sector += sector_count;
        numSectors -= sector_count;
        buffer = (u8*)buffer + (0x200 * sector_count);
    }
    return true;
}

bool _CF_readSectors(u32 sector, u32 numSectors, void* buffer) {
	return CF_PerformTransfer(CF_CMD_READ, sector, numSectors, buffer);
}

bool _CF_writeSectors(u32 sector, u32 numSectors, void* buffer) {
	return CF_PerformTransfer(CF_CMD_WRITE, sector, numSectors, buffer);
}

bool _CF_shutdown(void) { return _CF_clearStatus(); }

bool _CF_startup() {
    u16 temp = CF_REG_LBA1;
    CF_REG_LBA1 = (~temp & 0xFF);
    temp = (~temp & 0xFF);
    if (!(CF_REG_LBA1 == temp)) {
        return false;
    }

    CF_REG_LBA1 = 0xAA55;
    if (CF_REG_LBA1 == 0xAA55) {
        return false;
    }
    return true;
}
