#ifndef IO_CF_REGS_H
#define IO_CF_REGS_H

//---------------------------------------------------------------
// M3 CF Addresses
#define REG_M3CF_STS		*((vu16*)0x080C0000)	// Status of the CF Card / Device control
#define REG_M3CF_CMD		*((vu16*)0x088E0000)	// Commands sent to control chip and status return
#define REG_M3CF_ERR		*((vu16*)0x08820000)	// Errors / Features

#define REG_M3CF_SEC		*((vu16*)0x08840000)	// Number of sector to transfer
#define REG_M3CF_LBA1		*((vu16*)0x08860000)	// 1st byte of sector address
#define REG_M3CF_LBA2		*((vu16*)0x08880000)	// 2nd byte of sector address
#define REG_M3CF_LBA3		*((vu16*)0x088A0000)	// 3rd byte of sector address
#define REG_M3CF_LBA4		*((vu16*)0x088C0000)	// last nibble of sector address | 0xE0

#define REG_M3CF_DATA		*((vu16*)0x08800000)		// Pointer to buffer of CF data transered from card

#define CF_REG_DATA			REG_M3CF_DATA
#define CF_REG_STATUS		REG_M3CF_STS
#define CF_REG_COMMAND		REG_M3CF_CMD
#define CF_REG_ERROR		REG_M3CF_ERR
#define CF_REG_SECTOR_COUNT	REG_M3CF_SEC
#define CF_REG_LBA1			REG_M3CF_LBA1
#define CF_REG_LBA2			REG_M3CF_LBA2
#define CF_REG_LBA3			REG_M3CF_LBA3
#define CF_REG_LBA4			REG_M3CF_LBA4

#endif // define IO_CF_REGS_H
