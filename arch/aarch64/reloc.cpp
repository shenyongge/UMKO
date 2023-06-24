#include <stdint.h>
#include <errno.h>
#include <elfio/elfio.hpp>
#include "logger.h"
#include "reloc.h"


using namespace ELFIO;
/* Preparations for inclusion of some Linux kernel routines */

#define fallthrough __attribute__((__fallthrough__))
#define BIT(nr) (1UL << (nr))

#define cpu_to_le32
#define le32_to_cpu

#define FAULT_BRK_IMM		0x100
#define AARCH64_BREAK_MON	0xd4200000
#define AARCH64_BREAK_FAULT	(AARCH64_BREAK_MON | (FAULT_BRK_IMM << 5))

#define SZ_2M			0x00200000

#define ADR_IMM_HILOSPLIT	2
#define ADR_IMM_SIZE		SZ_2M
#define ADR_IMM_LOMASK		((1 << ADR_IMM_HILOSPLIT) - 1)
#define ADR_IMM_HIMASK		((ADR_IMM_SIZE >> ADR_IMM_HILOSPLIT) - 1)
#define ADR_IMM_LOSHIFT		29
#define ADR_IMM_HISHIFT		5


#define __aligned_u64  uint64_t __attribute__((aligned(8)))
#define __aligned_be64 uint64_t __attribute__((aligned(8)))
#define __aligned_le64 uint64_t __attribute__((aligned(8)))

#define CATENATE(x, y) x##y
#define CAT(x, y) CATENATE(x, y)

#define BUILD_BUG_ON(cond)	\
	enum { CAT(assert_line, __COUNTER__) = sizeof(int[-!!(cond)]) }

#include "insn.h"

static inline bool is_forbidden_offset_for_adrp(void *place)
{
	return false;
}

/* Following routines copied nearly verbatim from Linux kernel */
/* arch/arm64/kernel/module.c */

/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AArch64 loadable module support.
 *
 * Copyright (C) 2012 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

enum aarch64_reloc_op {
	RELOC_OP_NONE,
	RELOC_OP_ABS,
	RELOC_OP_PREL,
	RELOC_OP_PAGE,
};

static uint64_t do_reloc(enum aarch64_reloc_op reloc_op, uint32_t *place, uint64_t val)
{
	switch (reloc_op) {
	case RELOC_OP_NONE:
		return 0;
	case RELOC_OP_ABS:
		return val;
	case RELOC_OP_PREL:
		return val - (uint64_t)place;
	case RELOC_OP_PAGE:
		return (val & ~0xfff) - ((uint64_t)place & ~0xfff);
	}

	log_error("do_reloc: unknown relocation operation %d\n", reloc_op);
	return 0;
}

static int reloc_data(enum aarch64_reloc_op op, void *place, uint64_t val, int len)
{
	int64_t sval = do_reloc(op, (uint32_t *)place, val);

	/*
	 * The ELF psABI for AArch64 documents the 16-bit and 32-bit place
	 * relative and absolute relocations as having a range of [-2^15, 2^16)
	 * or [-2^31, 2^32), respectively. However, in order to be able to
	 * detect overflows reliably, we have to choose whether we interpret
	 * such quantities as signed or as unsigned, and stick with it.
	 * The way we organize our address space requires a signed
	 * interpretation of 32-bit relative references, so let's use that
	 * for all R_AARCH64_PRELxx relocations. This means our upper
	 * bound for overflow detection should be Sxx_MAX rather than Uxx_MAX.
	 */

	switch (len) {
	case 16:
		*(int16_t *)place = sval;
		switch (op) {
		case RELOC_OP_ABS:
			if (sval < 0 || sval > UINT16_MAX)
				return -ERANGE;
			break;
		case RELOC_OP_PREL:
			if (sval < INT16_MIN || sval > INT16_MAX)
				return -ERANGE;
			break;
		default:
			log_error("Invalid 16-bit data relocation (%d)\n", op);
			return 0;
		}
		break;
	case 32:
		*(int32_t *)place = sval;
		switch (op) {
		case RELOC_OP_ABS:
			if (sval < 0 || sval > UINT32_MAX)
				return -ERANGE;
			break;
		case RELOC_OP_PREL:
			if (sval < INT32_MIN || sval > INT32_MAX)
				return -ERANGE;
			break;
		default:
			log_error("Invalid 32-bit data relocation (%d)\n", op);
			return 0;
		}
		break;
	case 64:
		*(int64_t *)place = sval;
		break;
	default:
		log_error("Invalid length (%d) for data relocation\n", len);
		return 0;
	}
	return 0;
}

static int aarch64_get_imm_shift_mask(enum aarch64_insn_imm_type type,
						uint32_t *maskp, int *shiftp)
{
	uint32_t mask;
	int shift;

	switch (type) {
	case AARCH64_INSN_IMM_26:
		mask = BIT(26) - 1;
		shift = 0;
		break;
	case AARCH64_INSN_IMM_19:
		mask = BIT(19) - 1;
		shift = 5;
		break;
	case AARCH64_INSN_IMM_16:
		mask = BIT(16) - 1;
		shift = 5;
		break;
	case AARCH64_INSN_IMM_14:
		mask = BIT(14) - 1;
		shift = 5;
		break;
	case AARCH64_INSN_IMM_12:
		mask = BIT(12) - 1;
		shift = 10;
		break;
	case AARCH64_INSN_IMM_9:
		mask = BIT(9) - 1;
		shift = 12;
		break;
	case AARCH64_INSN_IMM_7:
		mask = BIT(7) - 1;
		shift = 15;
		break;
	case AARCH64_INSN_IMM_6:
	case AARCH64_INSN_IMM_S:
		mask = BIT(6) - 1;
		shift = 10;
		break;
	case AARCH64_INSN_IMM_R:
		mask = BIT(6) - 1;
		shift = 16;
		break;
	case AARCH64_INSN_IMM_N:
		mask = 1;
		shift = 22;
		break;
	default:
		return -EINVAL;
	}

	*maskp = mask;
	*shiftp = shift;

	return 0;
}

enum aarch64_insn_movw_imm_type {
	AARCH64_INSN_IMM_MOVNZ,
	AARCH64_INSN_IMM_MOVKZ,
};

uint32_t aarch64_insn_encode_immediate(enum aarch64_insn_imm_type type,
				  uint32_t insn, uint64_t imm)
{
	uint32_t immlo, immhi, mask;
	int shift;

	if (insn == AARCH64_BREAK_FAULT)
		return AARCH64_BREAK_FAULT;

	switch (type) {
	case AARCH64_INSN_IMM_ADR:
		shift = 0;
		immlo = (imm & ADR_IMM_LOMASK) << ADR_IMM_LOSHIFT;
		imm >>= ADR_IMM_HILOSPLIT;
		immhi = (imm & ADR_IMM_HIMASK) << ADR_IMM_HISHIFT;
		imm = immlo | immhi;
		mask = ((ADR_IMM_LOMASK << ADR_IMM_LOSHIFT) |
			(ADR_IMM_HIMASK << ADR_IMM_HISHIFT));
		break;
	default:
		if (aarch64_get_imm_shift_mask(type, &mask, &shift) < 0) {
			log_error("aarch64_insn_encode_immediate: unknown immediate encoding %d\n", type);
			return AARCH64_BREAK_FAULT;
		}
	}

	/* Update the immediate field. */
	insn &= ~(mask << shift);
	insn |= (imm & mask) << shift;

	return insn;
}

static int reloc_insn_movw(enum aarch64_reloc_op op, uint32_t *place, uint64_t val,
			   int lsb, enum aarch64_insn_movw_imm_type imm_type)
{
	uint64_t imm;
	int64_t sval;
	uint32_t insn = le32_to_cpu(*place);

	sval = do_reloc(op, place, val);
	imm = sval >> lsb;

	if (imm_type == AARCH64_INSN_IMM_MOVNZ) {
		/*
		 * For signed MOVW relocations, we have to manipulate the
		 * instruction encoding depending on whether or not the
		 * immediate is less than zero.
		 */
		insn &= ~(3 << 29);
		if (sval >= 0) {
			/* >=0: Set the instruction to MOVZ (opcode 10b). */
			insn |= 2 << 29;
		} else {
			/*
			 * <0: Set the instruction to MOVN (opcode 00b).
			 *     Since we've masked the opcode already, we
			 *     don't need to do anything other than
			 *     inverting the new immediate field.
			 */
			imm = ~imm;
		}
	}

	/* Update the instruction with the new encoding. */
	insn = aarch64_insn_encode_immediate(AARCH64_INSN_IMM_16, insn, imm);
	*place = cpu_to_le32(insn);

	if (imm > UINT16_MAX)
		return -ERANGE;

	return 0;
}

static int reloc_insn_imm(enum aarch64_reloc_op op, uint32_t *place, uint64_t val,
			  int lsb, int len, enum aarch64_insn_imm_type imm_type)
{
	uint64_t imm, imm_mask;
	int64_t sval;
	uint32_t insn = le32_to_cpu(*place);

	/* Calculate the relocation value. */
	sval = do_reloc(op, place, val);
	sval >>= lsb;

	/* Extract the value bits and shift them to bit 0. */
	imm_mask = (BIT(lsb + len) - 1) >> lsb;
	imm = sval & imm_mask;

	/* Update the instruction's immediate field. */
	insn = aarch64_insn_encode_immediate(imm_type, insn, imm);
	*place = cpu_to_le32(insn);

	/*
	 * Extract the upper value bits (including the sign bit) and
	 * shift them to bit 0.
	 */
	sval = (int64_t)(sval & ~(imm_mask >> 1)) >> (len - 1);

	/*
	 * Overflow has occurred if the upper bits are not all equal to
	 * the sign bit of the value.
	 */
	if ((uint64_t)(sval + 1) >= 2)
		return -ERANGE;

	return 0;
}

static int reloc_insn_adrp(uint32_t *place, uint64_t val)
{
	uint32_t insn;

	if (!is_forbidden_offset_for_adrp(place))
		return reloc_insn_imm(RELOC_OP_PAGE, place, val, 12, 21,
				      AARCH64_INSN_IMM_ADR);

	/* patch ADRP to ADR if it is in range */
	if (!reloc_insn_imm(RELOC_OP_PREL, place, val & ~0xfff, 0, 21,
			    AARCH64_INSN_IMM_ADR)) {
		insn = le32_to_cpu(*place);
		insn &= ~BIT(31);
	} else {
		/* don't emit a veneer */
		return -ENOEXEC;
	}

	*place = cpu_to_le32(insn);
	return 0;
}

int do_relocate_add(uint32_t reloc_type, void *ins_loc, uint64_t val)
{
	bool overflow_check = true;
	int ovf;
	uint32_t *loc = (uint32_t *)ins_loc;
	/* Perform the static relocation. */
	switch (reloc_type) {
	/* Null relocations. */
	case R_AARCH64_NONE:
		ovf = 0;
		break;

	/* Data relocations. */
	case R_AARCH64_ABS64:
		overflow_check = false;
		ovf = reloc_data(RELOC_OP_ABS, loc, val, 64);
		break;
	case R_AARCH64_P32_ABS32:
	case R_AARCH64_ABS32:
		ovf = reloc_data(RELOC_OP_ABS, loc, val, 32);
		break;
	case R_AARCH64_P32_ABS16:
	case R_AARCH64_ABS16:
		ovf = reloc_data(RELOC_OP_ABS, loc, val, 16);
		break;
	case R_AARCH64_PREL64:
		overflow_check = false;
		ovf = reloc_data(RELOC_OP_PREL, loc, val, 64);
		break;
	case R_AARCH64_P32_PREL32:
	case R_AARCH64_PREL32:
		ovf = reloc_data(RELOC_OP_PREL, loc, val, 32);
		break;
	case R_AARCH64_P32_PREL16:
	case R_AARCH64_PREL16:
		ovf = reloc_data(RELOC_OP_PREL, loc, val, 16);
		break;

	/* MOVW instruction relocations. */
	case R_AARCH64_P32_MOVW_UABS_G0_NC:
	case R_AARCH64_MOVW_UABS_G0_NC:
		overflow_check = false;
		fallthrough;
	case R_AARCH64_P32_MOVW_UABS_G0:
	case R_AARCH64_MOVW_UABS_G0:
		ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 0,
						AARCH64_INSN_IMM_MOVKZ);
		break;
	case R_AARCH64_MOVW_UABS_G1_NC:
		overflow_check = false;
		fallthrough;
	case R_AARCH64_P32_MOVW_UABS_G1:
	case R_AARCH64_MOVW_UABS_G1:
		ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 16,
						AARCH64_INSN_IMM_MOVKZ);
		break;
	case R_AARCH64_MOVW_UABS_G2_NC:
		overflow_check = false;
		fallthrough;
	case R_AARCH64_MOVW_UABS_G2:
		ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 32,
						AARCH64_INSN_IMM_MOVKZ);
		break;
	case R_AARCH64_MOVW_UABS_G3:
		/* We're using the top bits so we can't overflow. */
		overflow_check = false;
		ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 48,
						AARCH64_INSN_IMM_MOVKZ);
		break;
	case R_AARCH64_P32_MOVW_SABS_G0:
	case R_AARCH64_MOVW_SABS_G0:
		ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 0,
						AARCH64_INSN_IMM_MOVNZ);
		break;
	case R_AARCH64_MOVW_SABS_G1:
		ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 16,
						AARCH64_INSN_IMM_MOVNZ);
		break;
	case R_AARCH64_MOVW_SABS_G2:
		ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 32,
						AARCH64_INSN_IMM_MOVNZ);
		break;
	case R_AARCH64_P32_MOVW_PREL_G0_NC:
	case R_AARCH64_MOVW_PREL_G0_NC:
		overflow_check = false;
		ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 0,
						AARCH64_INSN_IMM_MOVKZ);
		break;
	case R_AARCH64_P32_MOVW_PREL_G0:
	case R_AARCH64_MOVW_PREL_G0:
		ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 0,
						AARCH64_INSN_IMM_MOVNZ);
		break;
	case R_AARCH64_MOVW_PREL_G1_NC:
		overflow_check = false;
		ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 16,
						AARCH64_INSN_IMM_MOVKZ);
		break;
	case R_AARCH64_P32_MOVW_PREL_G1:
	case R_AARCH64_MOVW_PREL_G1:
		ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 16,
						AARCH64_INSN_IMM_MOVNZ);
		break;
	case R_AARCH64_MOVW_PREL_G2_NC:
		overflow_check = false;
		ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 32,
						AARCH64_INSN_IMM_MOVKZ);
		break;
	case R_AARCH64_MOVW_PREL_G2:
		ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 32,
						AARCH64_INSN_IMM_MOVNZ);
		break;
	case R_AARCH64_MOVW_PREL_G3:
		/* We're using the top bits so we can't overflow. */
		overflow_check = false;
		ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 48,
						AARCH64_INSN_IMM_MOVNZ);
		break;

	/* Immediate instruction relocations. */
	case R_AARCH64_P32_LD_PREL_LO19:
	case R_AARCH64_LD_PREL_LO19:
		ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2, 19,
						AARCH64_INSN_IMM_19);
		break;
	case R_AARCH64_P32_ADR_PREL_LO21:
	case R_AARCH64_ADR_PREL_LO21:
		ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 0, 21,
						AARCH64_INSN_IMM_ADR);
		break;
	case R_AARCH64_ADR_PREL_PG_HI21_NC:
		overflow_check = false;
		fallthrough;
	case R_AARCH64_P32_ADR_PREL_PG_HI21:
	case R_AARCH64_ADR_PREL_PG_HI21:
		ovf = reloc_insn_adrp(loc, val);
		if (ovf && ovf != -ERANGE) {
			return ovf;
		}
		break;
	case R_AARCH64_P32_ADD_ABS_LO12_NC:
	case R_AARCH64_ADD_ABS_LO12_NC:
	case R_AARCH64_LDST8_ABS_LO12_NC:
		overflow_check = false;
		ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 0, 12,
						AARCH64_INSN_IMM_12);
		break;
	case R_AARCH64_P32_LDST16_ABS_LO12_NC:
	case R_AARCH64_LDST16_ABS_LO12_NC:
		overflow_check = false;
		ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 1, 11,
						AARCH64_INSN_IMM_12);
		break;
	case R_AARCH64_P32_LDST32_ABS_LO12_NC:
	case R_AARCH64_LDST32_ABS_LO12_NC:
		overflow_check = false;
		ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 2, 10,
						AARCH64_INSN_IMM_12);
		break;
	case R_AARCH64_P32_LDST64_ABS_LO12_NC:
	case R_AARCH64_LDST64_ABS_LO12_NC:
		overflow_check = false;
		ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 3, 9,
						AARCH64_INSN_IMM_12);
		break;
	case R_AARCH64_P32_LDST128_ABS_LO12_NC:
	case R_AARCH64_LDST128_ABS_LO12_NC:
		overflow_check = false;
		ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 4, 8,
						AARCH64_INSN_IMM_12);
		break;
	case R_AARCH64_P32_TSTBR14:
	case R_AARCH64_TSTBR14:
		ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2, 14,
						AARCH64_INSN_IMM_14);
		break;
	case R_AARCH64_P32_CONDBR19:
	case R_AARCH64_CONDBR19:
		ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2, 19,
						AARCH64_INSN_IMM_19);
		break;
	case R_AARCH64_P32_JUMP26:
	case R_AARCH64_P32_CALL26:
	case R_AARCH64_JUMP26:
	case R_AARCH64_CALL26:
		ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2, 26,
						AARCH64_INSN_IMM_26);
		break;
	default:
		log_error("unsupported RELA relocation: type: %u, loc: %p, val:0x%lx\n",
				reloc_type, loc, val);
		return -ENOEXEC;
	}

	if (overflow_check && ovf == -ERANGE) {
		log_warn("overflow in relocation type:%d, loc:%p, val:0x%lx\n",
			reloc_type, loc, val);
		return -ENOEXEC;
	}
	
	return 0;
}
