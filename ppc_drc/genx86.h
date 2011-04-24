typedef enum
{
	JUMP_TYPE_NONE		= 0,
	JUMP_TYPE_BACKWARDS	= 1,
	JUMP_TYPE_FORWARD	= 2,
} JUMP_TYPE;

typedef struct
{
	void *pointer;
	JUMP_TYPE jump_type;
} JUMP_TARGET;

enum
{
	REG_EAX = 0,
	REG_ECX = 1,
	REG_EDX = 2,
	REG_EBX = 3,
	REG_ESP = 4,
	REG_EBP = 5,
	REG_ESI = 6,
	REG_EDI = 7
} X86_REGS;

enum
{
	REG_AL = 0,
	REG_CL = 1,
	REG_DL = 2,
	REG_BL = 3,
	REG_AH = 4,
	REG_CH = 5,
	REG_DH = 6,
	REG_BH = 7
} X86_REGS8;

enum
{
	REG_AX = 0,
	REG_CX = 1,
	REG_DX = 2,
	REG_BX = 3,
	REG_SP = 4,
	REG_BP = 5,
	REG_SI = 6,
	REG_DI = 7
} X86_REGS16;

enum
{
	REG_XMM0 = 0,
	REG_XMM1 = 1,
	REG_XMM2 = 2,
	REG_XMM3 = 3,
	REG_XMM4 = 4,
	REG_XMM5 = 5,
	REG_XMM6 = 6,
	REG_XMM7 = 7,
} X86_REGS_SSE2;

typedef enum
{
	ADD,
	ADDI,
	ADDIM,
	ADDMR,
	ADC,
	ADCI,
	ADCMR,
	AND,
	ANDI,
	ANDIM,
	ANDMR,
	BSR,
	BSWAP,
	BTIM,
	BTRI,
	CALL,
	CALLI,
	CMOVAMR,
	CMOVBMR,
	CMOVGMR,
	CMOVLMR,
	CMOVZMR,
	CMP,
	CMPI,
	CMPIM,
	CMPMR,
	CVTSD2SS,
	CVTSS2SD,
	FABS,
	FCHS,
	FADDM64,
	FDIVM64,
	FLDM64,
	FSTPM64,
	IDIV,
	IMUL,
	JA,
	JAE,
	JB,
	JG,
	JL,
	JMP,
	JMPI,
	JMPM,
	JMPR,
	JNS,
	JNZ,
	JZ,
	MOV,
	MOVI,
	MOVIM,
	MOVMR,
	MOVRM,
	MOVR8M8,
	MOVM8R8,
	MOVS_R8R32,
	MOVS_R16R32,
	MOVZ_M8R32,
	MOVZ_R8R32,
	MOVZ_R16R32,
	MOVDRX,
	MOVDXR,
	MOVQMX,
	MOVQXM,
	MUL,
	NEG,
	NOT,
	OR,
	ORI,
	ORIM,
	ORMR,
	POP,
	POPAD,
	PUSH,
	PUSHAD,
	PUSHI,
	RET,
	ROLCL,
	ROLI,
	SETCR8,
	SETNCR8,
	SETZR8,
	SHLCL,
	SHLI,
	SHRCL,
	SHRI,
	SUB,
	SUBI,
	SUBIM,
	SUBMR,
	TESTI,
	XCHGR8R8,
	XOR,
	XORI,
	XORMR,
} GENX86_OPCODE;

void gen(GENX86_OPCODE opcode, INT32 dst_param, INT32 src_param);
void gen_jmp_target(JUMP_TARGET *target);
void gen_jmp(GENX86_OPCODE opcode, JUMP_TARGET *target);
void gen_jmp_rpr(INT32 reg1, INT32 reg2);
void gen_mov_dpr_to_reg(INT32 dst_reg, INT32 disp, INT32 disp_reg);
void gen_mov_dprs_to_reg(INT32 dst_reg, INT32 disp, INT32 disp_reg, INT32 disp_reg_scale);
void gen_mov_reg_to_dpr(INT32 src_reg, INT32 disp, INT32 disp_reg);
void gen_reset_cache(void);
UINT8 *gen_get_cache_pos(void);
UINT32 gen_get_instruction_amount(void);
void init_jmp_target(JUMP_TARGET *target);
void init_genx86(void);
void shutdown_genx86(void);