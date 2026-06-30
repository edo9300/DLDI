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

.macro CALL_NO_INTERWORK fncreg
    mov lr,pc
    mov pc,\fncreg
.endm

@ returns in r2, doesn't touch other regs
@ ez5h_sendCommand(u32 byteswapped_low, u32 non_byteswapped_high) -> u8
BEGIN_ASM_FUNC ez5h_sendCommand
	push {r1,r3-r5}
	
	adr r2, ez5h_sendCommand_data
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
	pop {r1,r3-r5}
	mov pc, lr

@ ez5h_sendSDIOCommand(u8 command, u32 parameter)
@ returns either 0 or EZ5H_CMD_SDMC_SEND_CLK(1) (r0-r1)
BEGIN_ASM_FUNC_NO_SECTION ez5h_sendSDIOCommand
	push {r2-r7}
	mov r8, lr
	lsls r2, r0, #24
	@ fixed part of the EZ5H_CMD_SDMC_SDIO command
	ldr r7, =0x0000FAB8
	@ this is equivalent to an OR, since the values don't overlap, but we need an ADD instruction to use 3 regs
	adds r0, r7, r2
	@ r1 is passed as is, not byteswapped, while r0 is constructed already byteswapped
	bl ez5h_sendCommand
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
	@ ez5h_sendCommand leaves r0-r1 intact and returns in r2
	bl ez5h_sendCommand
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
	pop {r2-r7}
	mov pc, r8

.balign 4
ez5h_sendCommand_data:
	.word   REG_MCCMD0
	.word   EZ5H_CTRL_READ_4B
	.word   REG_MCD1

@ bool ez5h_readSector(u32 sector, void* buffer)
BEGIN_ASM_FUNC ez5h_readSector
	push {r4-r7,lr}
	movs r6,r1

	adr r2,read_sector_data
	@ r2 holds the lower word of EZ5H_CMD_SDMC_READ_DATA
	@ r3 holds REG_MCCMD0
	@ r4 holds EZ5H_CTRL_READ_512
	@ r5 holds REG_MCD1
	@ r7 holds the address to ez5h_sendSDIOCommand
	ldmia r2, {r2,r3,r4,r5,r7}

.global ez5h_sdhc_read_label
ez5h_sdhc_read_label:
	lsls r1,r0,#9

	movs r0,#0x51
    @ call ez5h_sendSDIOCommand
    bl tramp
	cmp r0,#0
	beq sdio_fail

	@ lower word of EZ5H_CMD_SDMC_READ_DATA
	movs r7, #0

	stmia r3!, {r2, r7}
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
tramp:
	bx r7
.balign 4
read_sector_data:
	.word EZ5H_CMD_SDMC_READ_DATA_LOWER_WORD
	.word REG_MCCMD0
	.word EZ5H_CTRL_READ_512
	.word REG_MCD1
.global read_ez5h_sendSDIOCommand_label
read_ez5h_sendSDIOCommand_label:
	.word 0

@ bool ez5h_writeSector(u32 sector, void* buffer)
BEGIN_ASM_FUNC ez5h_writeSector
	push {r0-r1,r4-r7,lr}

.global ez5h_sdhc_write_label
ez5h_sdhc_write_label:
	lsls r1, r0, #9

	movs r0, #0x58
	ldr r7, ez5h_writeSector_sendCommand
	@ sendCommand+0x28 = ez5h_writeSector_sendSDIOCommand
	adds r7, 0x2A
	@ bl write_trampoline
	CALL_NO_INTERWORK r7
	cmp r0, #0
	beq sdio_fail_write

	@ ez5h_sendSDIOCommand returned us EZ5H_CMD_SDMC_SEND_CLK(1) in r0-r1
	@ save low word of command
	movs r6, r0
	@ subs r7, 0x28
	ldr r7, ez5h_writeSector_sendCommand
	@ bl write_trampoline
	CALL_NO_INTERWORK r7
	@ bl ez5h_sendCommand

	@ we use lower short as value to write, upper short is EZ5H_CMD_SDMC_SEND_CRC_STATUS used below
	ldr r1, =0xF8B8F0FF
	lsrs r5, r1, #16

	@ bl ez5h_sendWriteDataRomCommandShort
	ldr r7, ez5h_writeSector_sendWriteDataRomCommand
	adds r4, r7, #4
	CALL_NO_INTERWORK r4
	@ bl write_trampoline

	@ load buffer addr that was pushed at the start
	@ sdio4BitCrc16 will get the arguments directly from the stack
	@ and return the buffer address in r0
	@ bl ez5h_sdio4BitCrc16
	ldr r4, ez5h_writeSector_sdio4BitCrc16
	CALL_NO_INTERWORK r4
	@ ldr r7, ez5h_writeSector_sdio4BitCrc16
	@ bl write_trampoline

	@ write the data
	@ r0 is the data buffer left untouched by the above function call
	@ and it gets automatically incremented in ez5h_sendWriteDataRomCommand
	@ ldr r7, ez5h_writeSector_sendWriteDataRomCommand
	movs r3, #0xFF
1:
	@ bl ez5h_sendWriteDataRomCommand
	@ bl write_trampoline
	CALL_NO_INTERWORK r7
	@ do 0x100 iterations
	subs r3, #1
	bge 1b

	@ write the crc
	@ r0 gets automatically incremented in ez5h_sendWriteDataRomCommand
	mov r0, sp
	movs r3, #4
1:
	@ bl ez5h_sendWriteDataRomCommand
	@ bl write_trampoline
	CALL_NO_INTERWORK r7
	subs r3, #1
	bne 1b

	ldr r7, ez5h_writeSector_sendCommand
	@ wait crc status start acknowledgment
	@ load EZ5H_CMD_SDMC_SEND_CRC_STATUS
	movs r1, #0
	movs r0, r5
1:
	@ bl ez5h_sendCommand
	@ bl write_trampoline
	CALL_NO_INTERWORK r7
	lsrs r2, #1
	bcs 1b

	@ send single crc read clock
	@ bl ez5h_sendCommand
	@ ===============================MAYBE BREAK====================
	@ bl write_trampoline

	@ wait crc status acknowledged
1:
	@ bl ez5h_sendCommand
	@ bl write_trampoline
	CALL_NO_INTERWORK r7
	lsrs r2, #1
	bcc 1b

	@ wait for card to be ready again
	@ load backed up EZ5H_CMD_SDMC_SEND_CLK(1), r1 is already setup as 0 from before
	movs r0, r6
	movs r4, #0xFF
1:
	@ bl ez5h_sendCommand
	@ bl write_trampoline
	CALL_NO_INTERWORK r7
	tst r2, r4
	bne 1b

sdio_fail_write:
	@ r0 either is 0 or is EZ5H_CMD_SDMC_SEND_CLK(1) (thus nonzero)
	pop	{r1-r2,r4-r7,pc}
@ write_trampoline:
	@ bx r7
.pool

.global ez5h_writeSector_sendCommand
.global ez5h_writeSector_sendWriteDataRomCommand
.global ez5h_writeSector_sdio4BitCrc16

ez5h_writeSector_sendCommand:
	.word 0
ez5h_writeSector_sendWriteDataRomCommand:
	.word 0
ez5h_writeSector_sdio4BitCrc16:
	.word 0


@ static uint64_t inline calSingleCRC16(uint64_t crc, uint32_t data_in){
@ 	// Shift out 8 bits for each line
@ 	uint32_t data_out = crc >> 32;
@ 	crc <<= 32;
@
@ 	// XOR outgoing data to itself with 4 bit delay
@ 	data_out ^= (data_out >> 16);
@
@ 	// XOR incoming data to outgoing data with 4 bit delay
@ 	data_out ^= (data_in >> 16);
@
@ 	// XOR outgoing and incoming data to accumulator at each tap
@ 	uint64_t xorred = data_out ^ data_in;
@ 	crc ^= xorred;
@ 	crc ^= xorred << (5 * 4);
@ 	crc ^= xorred << (12 * 4);
@ 	return crc;
@ }
@ void sdio_crc16_4bit_checksum(void* dataBuf, uint64_t* out)
@ {
@ 	uint32_t num_words = 512 / sizeof(uint32_t);
@ 	uint64_t crc = 0;
@     auto* data = static_cast<uint32_t*>(dataBuf);
@     auto* end = data + num_words;
@     while (data < end)
@     {
@         uint32_t data_in = __builtin_bswap32(*data++);
@         crc = calSingleCRC16(crc, data_in);
@     }
@
@ 	*out = __builtin_bswap64(crc);
@ }

@ void sdio_crc16_4bit_checksum(void* inbuff, uint64_t* out)
@ args are passed on the stack:
@ inbuff = sp+4
@ out = sp
BEGIN_ASM_FUNC ez5h_sdio4BitCrc16
	ldr r0, [sp,#4]
	mov r1, sp
    push {r0,r2,r3,r4-r5,r6,lr}
    movs r4, #0 @ r4 = crc_lo
    movs r5, #0 @ r5 = crc_hi
    movs r6, #128
1:
    @ r5 = data_out
    lsrs r3, r5, #16
    eors r5, r3

    ldmia r0!, {r2}

    bl byteSwap32
    @ r2 = data_in

    lsrs r3, r2, #16
    eors r5, r3
    eors r2, r5 // r2 = xorred
    movs r5, r4 // r5 = crc_hi
    movs r4, r2 // r4 = crc_lo

    lsls r3, r2, #20
    eors r4, r3
    lsrs r3, r2, #12
    eors r5, r3
    lsls r3, r2, #16
    eors r5, r3

    subs r6, #1
    bne 1b

    movs r2, r4
    bl byteSwap32
	str r2, [r1,#4]

    movs r2, r5
    bl byteSwap32
	str r2, [r1]
    pop {r0,r2,r3,r4-r5,r6}
	pop {r1}
    mov pc,r1

byteSwap32:
    push {r4-r5,lr}
    movs r5, #16
    ldr r4, =0xFF00FF
    rors r2, r5 // ror 16
    ands r4, r2
    bics r2, r4
    lsls r4, r4, #8
    lsrs r2, r2, #8
    orrs r2, r4
    pop {r4-r5,pc}

.balign 4

.pool

@ez5h_sendWriteDataRomCommand(const u8* datab)
BEGIN_ASM_FUNC ez5h_sendWriteDataRomCommand
	ldrh r1, [r0]
	adds r0, #2
BEGIN_ASM_FUNC_NO_SECTION ez5h_sendWriteDataRomCommandShort
	push {r0,r3}
	adr r0,send_writedata_data
	@ r0 holds EZ5H_CTRL_READ_0
	@ r2 holds the lower word of EZ5H_CMD_SDMC_WRITE_DATA 0xF6B8
	@ r3 holds REG_MCCNT0
	ldmia r0, {r0,r2-r3}

	@ REG_MCCNT0 + 8 = REG_MCCMD0, so offset all the next writes
	strh r2, [r3, #0+8]

	strb r1, [r3, #3+8]

	lsrs r1, #4
	strb r1, [r3, #2+8]

	lsrs r1, #4
	strb r1, [r3, #5+8]

	lsrs r1, #4
	strb r1, [r3, #4+8]
	
	@ REG_MCCNT0 is 0x040001A0, << 10 = 0xXXXX8000
	lsls r1, r3, #10
	strh r1, [r3]

	@ REG_MCCNT0 + 4 = REG_MCCNT1
	str r0, [r3, #4]

	@ check for busy
1:
	ldr r2, [r3, #4]
	@ check if bit 31 is set (busy flag)
	cmp r2, #0
	blt 1b
	pop {r0,r3}
	mov pc, lr

.balign 4
send_writedata_data:
	.word EZ5H_CTRL_READ_0
	.word 0xF6B8
	.word REG_MCCNT0

.arm
@ez5h_writeMultipleSector(u32 sector, u8 * buffer, u32 num_sectors)
BEGIN_ASM_FUNC ez5h_writeMultipleSector
	ldr	r3, ez5h_writeSector_addr
	b save_regs_and_switch_to_thumb

@ez5h_readMultipleSector(u32 sector, u8 * buffer, u32 num_sectors)
BEGIN_ASM_FUNC_NO_SECTION ez5h_readMultipleSector
	ldr	r3, ez5h_readSector_addr
save_regs_and_switch_to_thumb:
	push {r4-r12,lr}
	adr r4, doSDOperation
	orr r4, #1
	bl trampoline
	pop {r4-r12,lr}
	bx lr
trampoline:
	bx r4

.thumb
@ bool doOperation(uint32_t sector, uint32_t num_sectors, void* buffer, bool(*operation)(u32 sector, void* buffer))
@ BEGIN_ASM_FUNC_NO_SECTION doSDOperation thumb
doSDOperation:
	push {r3-r7, lr}
	movs r4, r0
	movs r5, r2
	movs r7, r3
	adds r6, r0, r1

check_next_sector:
	cmp r4, r6
	bne parse_next_sector

	movs r0, #0x1
sderror:
	pop {r3-r7,pc}

parse_next_sector:
	movs r1, r5
	movs r0, r4
	bl call_sd_function
	cmp r0, #0x0
	beq sderror

	movs r3, #0x80
	lsls r3, #0x2
	adds r5, r3
	adds r4, #0x1
	b check_next_sector

call_sd_function:
	bx	r7

.balign 4
.global ez5h_readSector_addr
ez5h_readSector_addr:
	.word 0
.global ez5h_writeSector_addr
ez5h_writeSector_addr:
	.word 0
