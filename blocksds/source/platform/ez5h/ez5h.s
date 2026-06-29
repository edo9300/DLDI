#include <nds/asminc.h>

.equ REG_MCCNT0 , 0x040001A0
.equ REG_MCD0   , 0x040001A2
.equ REG_MCCNT1 , 0x040001A4
.equ REG_MCCMD0 , 0x040001A8
.equ REG_MCCMD1 , 0x040001AC
.equ REG_MCSCR0 , 0x040001B0
.equ REG_MCSCR1 , 0x040001B4
.equ REG_MCSCR2 , 0x040001B8
.equ REG_MCD1   , 0x04100010

.syntax unified
.thumb

.equ EZ5H_CMD_SDMC_READ_DATA_LOWER_WORD, 0x0000F7B8
.equ EZ5H_CTRL_READ_0, 0xA0586000
.equ EZ5H_CTRL_READ_512, 0xA1586000
.equ EZ5H_CTRL_READ_4B, 0xA7586000

.macro CHECK_DATA_READY dstreg,srcreg,off,label
	@ read mccnt1 status flag
	ldr	 \dstreg, [\srcreg, \off]
	@ check that (r2 & (1 << 23)) (data ready), by shifting right 24 bits, if the bit was set, the carry gets updated
	lsrs \dstreg, #24
    bcc \label
.endm

@ bool EZ5H_SDReadSector(u32 sector, void* buffer)
BEGIN_ASM_FUNC EZ5H_SDReadSector
	push {r4-r7,lr}
	movs r6,r1

sdhc_read_label:
	lsls r1,r0,#9

	movs r0,#0x51
	bl EZ5H_SDSendSDIOCommand2
	cmp r0,#0
	beq sdio_fail

	adr r1,read_sector_data
	@ r1 holds the lower word of EZ5H_CMD_SDMC_READ_DATA
	@ r3 holds REG_MCCMD0
	@ r4 holds EZ5H_CTRL_READ_512
	@ r5 holds REG_MCD1
	ldmia r1, {r1,r3,r4,r5}

	@ lower word of EZ5H_CMD_SDMC_READ_DATA
	movs r2, #0

	stmia r3!, {r1, r2}
	@ REG_MCCMD0 is incremented by 8 in the stmia, REG_MCCMD0-8 = REG_MCCNT0
	subs r3, #16

	@ REG_MCCMD0 is 0x040001A0, << 10 = 0xXXXX8000
	lsls r1, r3, #10
	strh r1, [r3]

	@ REG_MCCNT00 + 4 = REG_MCCNT1
	@ write EZ5H_CTRL_READ_512 to mccnt1
	str r4, [r3, #4]	

	@ read data
	movs r2, #0x80
	lsls r2, r2, #2
	adds r1, r6, r2

is_busy:
	CHECK_DATA_READY r2,r3,#4,check_busy
	@ read mcd1 status flag
	ldr r2, [r5]
	cmp r1, r6
	bls check_busy
	stmia r6!, {r2}
check_busy:
	@ read mccnt1 status flag
	ldr r2, [r3,#4]
	@ check if bit 31 is set (busy flag)
	cmp r2, #0
	blt is_busy

sdio_fail:
	pop	 {r4-r7,pc}
.balign 4
read_sector_data:
	.word EZ5H_CMD_SDMC_READ_DATA_LOWER_WORD
	.word REG_MCCMD0
	.word EZ5H_CTRL_READ_512
	.word REG_MCD1

@ returns in r2, doesn't touch other regs
@ EZ5H_SendCommand(u32 byteswapped_low, u32 non_byteswapped_high) -> u8
BEGIN_ASM_FUNC EZ5H_SendCommand3
	push {r1,r3-r5,lr}
	
	adr r2, EZ5H_SendCommand_data
	@ r3 holds REG_MCCMD0
	@ r4 holds EZ5H_CTRL_READ_4B
	@ r5 holds REG_MCD1
	ldmia r2!, {r3,r4,r5}

	@ write card command, r0 is already byteswapped, r1 no
	stmia r3!, {r0}
	strb r1, [r3, #3]
	lsrs r1, #8
	strb r1, [r3, #2]
	lsrs r1, #8
	strb r1, [r3, #1]
	lsrs r1, #8
	strb r1, [r3, #0]
	@ REG_MCCMD0 is incremented by 8 in the stmia, REG_MCCMD0-8 = REG_MCCNT0
	subs r3, #12
	@ REG_MCCNT0 is 0x040001A0, << 10 = 0xXXXX8000
	lsls r2, r3, #10
	strh r2, [r3]

	@ REG_MCCNT0 + 4 = REG_MCCNT1
	@ write EZ5H_CTRL_READ_4B to mccnt1
	str r4, [r3, #4]	

1:
	CHECK_DATA_READY r1,r3,#4,1b

	@ read from REG_MCD1
	ldr r2, [r5]
	pop {r1,r3-r5,pc}

@ EZ5H_SDSendSDIOCommand2(u8 command, u32 parameter)
@ returns either 0 or EZ5H_CMD_SDMC_SEND_CLK(1) (r0-r1)
BEGIN_ASM_FUNC_NO_SECTION EZ5H_SDSendSDIOCommand2
	push {r4-r7,lr}
	lsls r2, r0, #24
	@ fixed part of the EZ5H_CMD_SDMC_SDIO command
	ldr r7, =0x0000FAB8
	@ this is equivalent to an OR, since the values don't overlap, but we need an ADD instruction to use 3 regs
	adds r0, r7, r2
	@ r1 is passed as is, not byteswapped, while r0 is constructed already byteswapped
	bl EZ5H_SendCommand3
	@ load 0x10000
	movs r4, #1
	lsls r4, #16
	@ r7 holds 0x0000FAB8
	@ this gives us the byteswapped card command equivalent to EZ5H_CMD_SDMC_PARAM_CARD(1, 0, 0), which is EZ5H_CMD_SDMC_SEND_CLK(1)
	@ with r0 being 0x0001FAB8 and r1 being 0x00000000
	adds r0, r4, r7
	@ lower part of the card command
	movs r1, #0

	movs r5, #0xFF
wait_for_start_marker:
	@ EZ5H_SendCommand3 leaves r0-r1 intact and returns in r2
	bl EZ5H_SendCommand3
	tst r2, r5
	bne start_marker_not_received
	@ r0 is already non-0
	@ movs r0, #1
	b end
start_marker_not_received:
	subs r4, #1
	bne wait_for_start_marker

	movs r0, #0
end:
	pop {r4-r7,pc}

.balign 4
EZ5H_SendCommand_data:
	.word   REG_MCCMD0
	.word   EZ5H_CTRL_READ_4B
	.word   REG_MCD1

@ bool EZ5H_SDWriteSector(u32 sector, void* buffer)
BEGIN_ASM_FUNC EZ5H_SDWriteSector
	push	{r0, r1, r4, r5, r6, r7, lr}
	movs	r5, r0
	movs	r0, r1
	movs	r4, r1
	bl	sdio_crc16_4bit_checksum
	str	r0, [sp]
	str	r1, [sp, #4]

sdhc_write_label:
	lsls	r1, r5, #9

	movs	r0, #0x58
	bl	EZ5H_SDSendSDIOCommand2
	subs	r5, r0, #0
	beq	.L2
	
	@ EZ5H_SDSendSDIOCommand2 returned us EZ5H_CMD_SDMC_SEND_CLK(1) in r0-r1
	@ movs	r1, #0
	@ ldr	r0, .L17
	@ push {r0,r1}
	movs r7, r0
	bl	EZ5H_SendCommand3

	@ we use lower short as value to write, upper short is EZ5H_CMD_SDMC_SEND_CRC_STATUS used below
	ldr	r0, =0xF8B8F0FF
	lsrs r5, r0, #16
	bl	cardExt_RomSendWriteDataShort
	movs	r3, #0x80
	lsls	r3, r3, #2
	adds	r6, r4, r3
.L3:
	movs	r0, r4
	adds	r4, r4, #2
	bl	cardExt_RomSendWriteData
	cmp	r4, r6
	bne	.L3
	movs	r4, #0
.L4:
	mov	r0, sp
	adds	r0, r0, r4
	adds	r4, r4, #2
	bl	cardExt_RomSendWriteData
	cmp	r4, #8
	bne	.L4
	subs	r4, r4, #7

	@ load EZ5H_CMD_SDMC_SEND_CRC_STATUS
	movs	r1, #0
	movs r0, r5
.L5:
	bl	EZ5H_SendCommand3
	movs	r3, r2
	ands	r3, r4
	tst	r2, r4
	bne	.L5
	bl	EZ5H_SendCommand3
	movs	r4, #1
.L6:
	bl	EZ5H_SendCommand3
	tst	r2, r4
	beq	.L6
	movs	r4, #0xFF
	@ pop {r0,r1}
.L7:
	@ ldr	r0, =0x0001FAB8
	@ load backed up EZ5H_CMD_SDMC_SEND_CLK(1)
	movs r0, r7
	bl	EZ5H_SendCommand3
	tst	r2, r4
	bne	.L7
.L2:
	@ sp needed
	@ r0 either is 0 or is EZ5H_CMD_SDMC_SEND_CLK(1) (thus nonzero)
	@ movs r0, r5
	pop	{r1, r2, r4, r5, r6, r7}
	pop	{r1}
	bx	r1
.pool


@cardExt_RomSendWriteData(const u8* datab)
BEGIN_ASM_FUNC cardExt_RomSendWriteData
	ldrh r0, [r0]
BEGIN_ASM_FUNC_NO_SECTION cardExt_RomSendWriteDataShort
	adr r1,send_writedata_data
	@ r1 holds EZ5H_CTRL_READ_0
	@ r2 holds the lower word of EZ5H_CMD_SDMC_WRITE_DATA 0xF6B8
	@ r3 holds REG_MCCNT0
	ldmia r1, {r1-r3}

	@ REG_MCCNT0 + 8 = REG_MCCMD0, so offset all the next writes
	strh r2, [r3, #0+8]

	strb r0, [r3, #3+8]

	lsrs r0, #4
	strb r0, [r3, #2+8]

	lsrs r0, #4
	strb r0, [r3, #5+8]

	lsrs r0, #4
	strb r0, [r3, #4+8]
	
	@ REG_MCCNT0 is 0x040001A0, << 10 = 0xXXXX8000
	lsls r0, r3, #10
	strh r0, [r3]

	@ REG_MCCNT0 + 4 = REG_MCCNT1
	str r1, [r3, #4]

	@ check for busy
1:
	ldr r2, [r3, #4]
	@ check if bit 31 is set (busy flag)
	cmp r2, #0
	blt 1b
	bx lr

.balign 4
send_writedata_data:
	.word EZ5H_CTRL_READ_0
	.word 0xF6B8
	.word REG_MCCNT0
