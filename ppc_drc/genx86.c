#include "model3.h"
#include "genx86.h"

static UINT8 *drc_cache_top;
static UINT8 *drc_cache;

#define DRC_CACHE_SIZE		0x800000

static UINT32 cumulative_instruction_amount;

static void emit_byte(UINT8 value)
{
	if (drc_cache_top - drc_cache > DRC_CACHE_SIZE)
	{
		error("gen: drc cache overflow!\n");
	}

	*drc_cache_top++ = value;
}

static void emit_dword(UINT32 value)
{
	if (drc_cache_top - drc_cache > DRC_CACHE_SIZE)
	{
		error("gen: drc cache overflow!\n");
	}

	*drc_cache_top++ = (value >>  0) & 0xff;
	*drc_cache_top++ = (value >>  8) & 0xff;
	*drc_cache_top++ = (value >> 16) & 0xff;
	*drc_cache_top++ = (value >> 24) & 0xff;
}

void gen(GENX86_OPCODE opcode, INT32 dst_param, INT32 src_param)
{
	cumulative_instruction_amount++;

	switch (opcode)
	{
		case ADD:
		{
			emit_byte(0x01);
			emit_byte(0xc0 | (src_param << 3) | (dst_param));
			break;
		}
		
		case ADDI:
		{
			// don't generate anything for dummy code
			if (src_param == 0)
				return;

			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0xc0 | dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0xc0 | dst_param);
				emit_dword(src_param);
			}
			break;	
		}

		case ADDIM:
		{
			// don't generate anything for dummy code
			if (src_param == 0)
				return;

			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0x05);
				emit_dword(dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0x05);
				emit_dword(dst_param);
				emit_dword(src_param);
			}
			break;
		}

		case ADDMR:
		{
			emit_byte(0x03);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}
		
		case ADC:
		{
			emit_byte(0x11);
			emit_byte(0xc0 | (src_param << 3) | (dst_param));
			break;
		}
		
		case ADCI:
		{
			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0xd0 | dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0xd0 | dst_param);
				emit_dword(src_param);
			}
			break;
		}

		case ADCMR:
		{
			emit_byte(0x13);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}
		
		case AND:
		{
			emit_byte(0x21);
			emit_byte(0xc0 | (src_param << 3) | (dst_param));
			break;
		}
		
		case ANDI:
		{
			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0xe0 | dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0xe0 | dst_param);
				emit_dword(src_param);
			}
			break;	
		}

		case ANDIM:
		{
			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0x25);
				emit_dword(dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0x25);
				emit_dword(dst_param);
				emit_dword(src_param);
			}
			break;
		}

		case ANDMR:
		{
			emit_byte(0x23);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case BSR:
		{
			emit_byte(0x0f);
			emit_byte(0xbd);
			emit_byte(0xc0 | (dst_param << 3) | src_param);
			break;
		}

		case BSWAP:
		{
			emit_byte(0x0f);
			emit_byte(0xc8 | dst_param);
			break;
		}

		case BTIM:
		{
			emit_byte(0x0f);
			emit_byte(0xba);
			emit_byte(0x25);
			emit_dword(dst_param);
			emit_byte(src_param);
			break;
		}

		case BTRI:
		{
			emit_byte(0x0f);
			emit_byte(0xba);
			emit_byte(0xf0 | dst_param);
			emit_byte(src_param);
			break;
		}
		
		case CALL:
		{
			emit_byte(0xff);
			emit_byte(0xd0 | dst_param);
			break;	
		}
		
		case CALLI:
		{
			UINT32 s = (UINT64)(drc_cache_top) + 5;
			emit_byte(0xe8);
			emit_dword((UINT32)(dst_param) - s);
			break;
		}

		case CMOVAMR:
		{
			emit_byte(0x0f);
			emit_byte(0x47);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case CMOVBMR:
		{
			emit_byte(0x0f);
			emit_byte(0x42);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case CMOVGMR:
		{
			emit_byte(0x0f);
			emit_byte(0x4f);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case CMOVLMR:
		{
			emit_byte(0x0f);
			emit_byte(0x4c);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case CMOVZMR:
		{
			emit_byte(0x0f);
			emit_byte(0x44);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}
		
		case CMP:
		{
			emit_byte(0x39);
			emit_byte(0xc0 | (src_param << 3) | (dst_param));
			break;
		}
		
		case CMPI:
		{
			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0xf8 | dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0xf8 | dst_param);
				emit_dword(src_param);
			}
			break;
		}

		case CMPIM:
		{
			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0x3d);
				emit_dword(dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0x3d);
				emit_dword(dst_param);
				emit_dword(src_param);
			}
			break;
		}

		case CMPMR:
		{
			emit_byte(0x3b);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case CVTSD2SS:
		{
			emit_byte(0xf2);
			emit_byte(0x0f);
			emit_byte(0x5a);
			emit_byte(0xc0 | (dst_param << 3) | src_param);
			break;
		}

		case CVTSS2SD:
		{
			emit_byte(0xf3);
			emit_byte(0x0f);
			emit_byte(0x5a);
			emit_byte(0xc0 | (dst_param << 3) | src_param);
			break;
		}

		case FABS:
		{
			emit_byte(0xd9);
			emit_byte(0xe1);
			break;
		}

		case FCHS:
		{
			emit_byte(0xd9);
			emit_byte(0xe0);
			break;
		}

		case FADDM64:
		{
			emit_byte(0xdc);
			emit_byte(0x05);
			emit_dword(dst_param);
			break;
		}

		case FDIVM64:
		{
			emit_byte(0xdc);
			emit_byte(0x35);
			emit_dword(dst_param);
			break;
		}

		case FLDM64:
		{
			emit_byte(0xdd);
			emit_byte(0x05);
			emit_dword(dst_param);
			break;
		}

		case FSTPM64:
		{
			emit_byte(0xdd);
			emit_byte(0x1d);
			emit_dword(dst_param);
			break;
		}

		case IDIV:
		{
			emit_byte(0xf7);
			emit_byte(0xf8 | dst_param);
			break;
		}

		case IMUL:
		{
			emit_byte(0xf7);
			emit_byte(0xe8 | dst_param);
			break;
		}

		case JMPI:
		{
			UINT32 s = (UINT64)(drc_cache_top) + 5;
			emit_byte(0xe9);
			emit_dword((UINT32)(dst_param) - s);
			return;
		}

		case JMPM:
		{
			emit_byte(0xff);
			emit_byte(0x25);
			emit_dword(dst_param);
			return;
		}

		case JMPR:
		{
			emit_byte(0xff);
			emit_byte(0xe0 | dst_param);
			return;
		}
		
		case MOV:
		{
			emit_byte(0x89);
			emit_byte(0xc0 | (src_param << 3) | (dst_param));
			break;	
		}
		
		case MOVI:
		{
			emit_byte(0xc7);
			emit_byte(0xc0 | dst_param);
			emit_dword(src_param);
			break;	
		}

		case MOVIM:
		{
			emit_byte(0xc7);
			emit_byte(0x05);
			emit_dword(dst_param);
			emit_dword(src_param);
			break;
		}

		case MOVMR:
		{
			emit_byte(0x8b);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case MOVRM:
		{
			emit_byte(0x89);
			emit_byte(0x05 | (src_param << 3));
			emit_dword(dst_param);
			break;
		}

		case MOVR8M8:
		{
			emit_byte(0x88);
			emit_byte(0x05 | (src_param << 3));
			emit_dword(dst_param);
			break;
		}

		case MOVM8R8:
		{
			emit_byte(0x8a);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}


		case MOVS_R8R32:
		{
			emit_byte(0x0f);
			emit_byte(0xbe);
			emit_byte(0xc0 | (dst_param << 3) | src_param);
			break;
		}

		case MOVS_R16R32:
		{
			emit_byte(0x0f);
			emit_byte(0xbf);
			emit_byte(0xc0 | (dst_param << 3) | src_param);
			break;
		}

		case MOVZ_M8R32:
		{
			emit_byte(0x0f);
			emit_byte(0xb6);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case MOVZ_R8R32:
		{
			emit_byte(0x0f);
			emit_byte(0xb6);
			emit_byte(0xc0 | (dst_param << 3) | src_param);
			break;
		}

		case MOVZ_R16R32:
		{
			emit_byte(0x0f);
			emit_byte(0xb7);
			emit_byte(0xc0 | (dst_param << 3) | src_param);
			break;
		}

		case MOVDRX:
		{
			emit_byte(0x66);
			emit_byte(0x0f);
			emit_byte(0x6e);
			emit_byte(0xc0 | (dst_param << 3) | src_param);
			break;
		}

		case MOVDXR:
		{
			emit_byte(0x66);
			emit_byte(0x0f);
			emit_byte(0x7e);
			emit_byte(0xc0 | (src_param << 3) | dst_param);
			break;
		}

		case MOVQMX:
		{
			emit_byte(0xf3);
			emit_byte(0x0f);
			emit_byte(0x7e);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case MOVQXM:
		{
			emit_byte(0x66);
			emit_byte(0x0f);
			emit_byte(0xd6);
			emit_byte(0x05 | (src_param << 3));
			emit_dword(dst_param);
			break;
		}

		case MUL:
		{
			emit_byte(0xf7);
			emit_byte(0xe0 | dst_param);
			break;
		}

		case NEG:
		{
			emit_byte(0xf7);
			emit_byte(0xd8 | dst_param);
			break;
		}

		case NOT:
		{
			emit_byte(0xf7);
			emit_byte(0xd0 | dst_param);
			break;
		}

		case OR:
		{
			emit_byte(0x09);
			emit_byte(0xc0 | (src_param << 3) | (dst_param));
			break;
		}

		case ORI:
		{
			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0xc8 | dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0xc8 | dst_param);
				emit_dword(src_param);
			}
			break;	
		}

		case ORIM:
		{
			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0x0d);
				emit_dword(dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0x0d);
				emit_dword(dst_param);
				emit_dword(src_param);
			}
			break;
		}

		case ORMR:
		{
			emit_byte(0x0b);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case POP:
		{
			emit_byte(0x58 | dst_param);
			break;	
		}

		case POPAD:
		{
			emit_byte(0x61);
			break;
		}
		
		case PUSH:
		{
			emit_byte(0x50 | dst_param);
			break;
		}

		case PUSHI:
		{
			emit_byte(0x68);
			emit_dword(dst_param);
			break;
		}

		case PUSHAD:
		{
			emit_byte(0x60);
			break;
		}
		
		case RET:
		{
			emit_byte(0xc3);
			break;
		}

		case ROLCL:
		{
			emit_byte(0xd3);
			emit_byte(0xc0 | dst_param);
			break;
		}

		case ROLI:
		{
			if (src_param == 1)
			{
				emit_byte(0xd1);
				emit_byte(0xc0 | dst_param);
			}
			else
			{
				emit_byte(0xc1);
				emit_byte(0xc0 | dst_param);
				emit_byte(src_param);
			}
			break;
		}

		case SETCR8:
		{
			emit_byte(0x0f);
			emit_byte(0x92);
			emit_byte(0xc0 | dst_param);
			break;
		}

		case SETNCR8:
		{
			emit_byte(0x0f);
			emit_byte(0x93);
			emit_byte(0xc0 | dst_param);
			break;
		}

		case SETZR8:
		{
			emit_byte(0x0f);
			emit_byte(0x94);
			emit_byte(0xc0 | dst_param);
			break;
		}

		case SHLCL:
		{
			emit_byte(0xd3);
			emit_byte(0xe0 | dst_param);
			break;
		}

		case SHLI:
		{
			if (src_param == 1)
			{
				emit_byte(0xd1);
				emit_byte(0xe0 | dst_param);
			}
			else
			{
				emit_byte(0xc1);
				emit_byte(0xe0 | dst_param);
				emit_byte(src_param);
			}
			break;
		}

		case SHRCL:
		{
			emit_byte(0xd3);
			emit_byte(0xe8 | dst_param);
			break;
		}

		case SHRI:
		{
			if (src_param == 1)
			{
				emit_byte(0xd1);
				emit_byte(0xe8 | dst_param);
			}
			else
			{
				emit_byte(0xc1);
				emit_byte(0xe8 | dst_param);
				emit_byte(src_param);
			}
			break;
		}
		
		case SUB:
		{
			emit_byte(0x29);
			emit_byte(0xc0 | (src_param << 3) | (dst_param));
			break;
		}
		
		case SUBI:
		{
			// don't generate anything for dummy code
			if (src_param == 0)
				return;

			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0xe8 | dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0xe8 | dst_param);
				emit_dword(src_param);
			}
			break;
		}

		case SUBIM:
		{
			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0x2d);
				emit_dword(dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0x2d);
				emit_dword(dst_param);
				emit_dword(src_param);
			}
			break;
		}

		case SUBMR:
		{
			emit_byte(0x2b);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}

		case TESTI:
		{
			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0xf6);
				emit_byte(0xc0 | dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0xf7);
				emit_byte(0xc0 | dst_param);
				emit_dword(src_param);
			}
			break;
		}

		case XCHGR8R8:
		{
			emit_byte(0x86);
			emit_byte(0xc0 | (dst_param << 3) | src_param);
			break;
		}

		case XOR:
		{
			emit_byte(0x31);
			emit_byte(0xc0 | (src_param << 3) | (dst_param));
			break;
		}

		case XORI:
		{
			if (src_param < 128 && src_param >= -128)
			{
				// 8-bit sign-extended immediate
				emit_byte(0x83);
				emit_byte(0xf0 | dst_param);
				emit_byte(src_param);
			}
			else
			{
				// 32-bit immediate
				emit_byte(0x81);
				emit_byte(0xf0 | dst_param);
				emit_dword(src_param);
			}
			break;	
		}

		case XORMR:
		{
			emit_byte(0x33);
			emit_byte(0x05 | (dst_param << 3));
			emit_dword(src_param);
			break;
		}
			
		default:
		{
			printf("DRC ERROR: gen: unknown opcode %08X\n", opcode);
			exit(1);
		}
	}
}

void gen_jmp_target(JUMP_TARGET *target)
{
	if (target->jump_type == JUMP_TYPE_NONE)
	{
		// target resolved first, this is a backwards jump
		target->jump_type = JUMP_TYPE_BACKWARDS;
		target->pointer = drc_cache_top;
	}
	else if (target->jump_type == JUMP_TYPE_FORWARD)
	{
		// source already resolved, backpatch the source jump
		UINT8 *d = (UINT8 *)(target->pointer);
		UINT8 *jump_offset = d - 4;
		INT32 jump_length = drc_cache_top - d;
		*jump_offset++ = (jump_length >>  0) & 0xff;
		*jump_offset++ = (jump_length >>  8) & 0xff;
		*jump_offset++ = (jump_length >> 16) & 0xff;
		*jump_offset++ = (jump_length >> 24) & 0xff;
	}
	else
	{
		printf("DRC ERROR: gen_jmp_target: something went wrong!\n");
		exit(1);
	}
}

void gen_jmp(GENX86_OPCODE opcode, JUMP_TARGET *target)
{
	int jump_size;
	UINT8 *s;

	cumulative_instruction_amount++;
	
	switch (opcode)
	{
		case JMP:
		{
			// TODO: check if 8-bit displacement is possible
			emit_byte(0xe9);
			jump_size = 4;
			break;
		}

		case JA:
		{
			emit_byte(0x0f);
			emit_byte(0x87);
			jump_size = 4;
			break;
		}

		case JAE:
		{
			emit_byte(0x0f);
			emit_byte(0x83);
			jump_size = 4;
			break;
		}

		case JB:
		{
			emit_byte(0x0f);
			emit_byte(0x82);
			jump_size = 4;
			break;
		}

		case JG:
		{
			emit_byte(0x0f);
			emit_byte(0x8f);
			jump_size = 4;
			break;
		}

		case JL:
		{
			emit_byte(0x0f);
			emit_byte(0x8c);
			jump_size = 4;
			break;
		}

		case JNS:
		{
			emit_byte(0x0f);
			emit_byte(0x89);
			jump_size = 4;
			break;
		}

		case JNZ:
		{
			// TODO: check if 8-bit displacement is possible
			emit_byte(0x0f);
			emit_byte(0x85);
			jump_size = 4;
			break;
		}

		case JZ:
		{
			emit_byte(0x0f);
			emit_byte(0x84);
			jump_size = 4;
			break;
		}

		default:
		{
			printf("DRC ERROR: gen_jmp: unknown opcode %08X\n", opcode);
			exit(1);
		}
	}
	
	// get the proper source location
	s = drc_cache_top + jump_size;
	
	if (target->jump_type == JUMP_TYPE_NONE)
	{
		// jump source resolved first, this is a forward jump
		target->jump_type = JUMP_TYPE_FORWARD;
		target->pointer = s;
		
		// emit the placeholder offset
		if (jump_size == 1)
		{
			emit_byte(0x00);
		}
		else if (jump_size == 4)
		{
			emit_dword(0x00000000);
		}
	}
	else if (target->jump_type == JUMP_TYPE_BACKWARDS)
	{
		// jump destination already resolved, generate the jump
		INT32 jump_length = (UINT64)(target->pointer) - (UINT64)(s);
		emit_dword(jump_length);
	}
	else
	{
		printf("DRC ERROR: gen_jmp: something went wrong!\n");
		exit(1);		
	}
}

void gen_jmp_rpr(INT32 reg1, INT32 reg2)
{
	emit_byte(0xff);
	emit_byte(0x20 | 0x04);
	emit_byte(reg2 << 3 | reg1);
}

void gen_mov_dpr_to_reg(INT32 dst_reg, INT32 disp, INT32 disp_reg)
{
	emit_byte(0x8b);
	emit_byte(0x80 | (dst_reg << 3) | (disp_reg));
	emit_dword(disp);
}

void gen_mov_dprs_to_reg(INT32 dst_reg, INT32 disp, INT32 disp_reg, INT32 disp_reg_scale)
{
	int scale;
	switch (disp_reg_scale)
	{
		case 2: scale = 1; break;
		case 4: scale = 2; break;
		case 8: scale = 3; break;
		default: scale = 0; break;
	}
	emit_byte(0x8b);
	emit_byte((dst_reg << 3) | 0x04);
	emit_byte(scale << 6 | disp_reg << 3 | 0x5);
	emit_dword(disp);
}

void gen_mov_reg_to_dpr(INT32 src_reg, INT32 disp, INT32 disp_reg)
{
	emit_byte(0x89);
	emit_byte(0x80 | (src_reg << 3) | (disp_reg));
	emit_dword(disp);
}

void gen_reset_cache(void)
{
	drc_cache_top = drc_cache;

	memset(drc_cache, 0, DRC_CACHE_SIZE);
}

UINT8 *gen_get_cache_pos(void)
{
	return drc_cache_top;
}

UINT32 gen_get_instruction_amount(void)
{
	return cumulative_instruction_amount;
}

void init_jmp_target(JUMP_TARGET *target)
{
	target->jump_type = JUMP_TYPE_NONE;
}

void init_genx86(void)
{
	drc_cache = malloc_exec(DRC_CACHE_SIZE);
	drc_cache_top = drc_cache;
}

void shutdown_genx86(void)
{
	// save cache
	//save_file("drccache.bin", drc_cache, 0x800000, FALSE);

	free_exec(drc_cache);
}
