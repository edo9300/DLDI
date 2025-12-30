#ifndef IO_CF_REGS_H
#define IO_CF_REGS_H

//---------------------------------------------------------------
// CF Addresses
#define REG_SCCF_STS		*((vu16*)0x098C0000)	// Status of the CF Card / Device control
#define REG_SCCF_CMD		*((vu16*)0x090E0000)	// Commands sent to control chip and status return
#define REG_SCCF_ERR		*((vu16*)0x09020000)	// Errors / Features

#define REG_SCCF_SEC		*((vu16*)0x09040000)	// Number of sector to transfer
#define REG_SCCF_LBA1		*((vu16*)0x09060000)	// 1st byte of sector address
#define REG_SCCF_LBA2		*((vu16*)0x09080000)	// 2nd byte of sector address
#define REG_SCCF_LBA3		*((vu16*)0x090A0000)	// 3rd byte of sector address
#define REG_SCCF_LBA4		*((vu16*)0x090C0000)	// last nibble of sector address | 0xE0

#define REG_SCCF_DATA		*((vu16*)0x09000000)	// Pointer to buffer of CF data transered from card

#define CF_REG_DATA			REG_SCCF_DATA
#define CF_REG_STATUS		REG_SCCF_STS
#define CF_REG_COMMAND		REG_SCCF_CMD
#define CF_REG_ERROR		REG_SCCF_ERR
#define CF_REG_SECTOR_COUNT	REG_SCCF_SEC
#define CF_REG_LBA1			REG_SCCF_LBA1
#define CF_REG_LBA2			REG_SCCF_LBA2
#define CF_REG_LBA3			REG_SCCF_LBA3
#define CF_REG_LBA4			REG_SCCF_LBA4

#endif // define IO_CF_REGS_H
