#ifndef OPCODE_H
#define OPCODE_H

typedef enum {
	INS_NOP = 0x30,
	INS_LOAD_CONST,
	INS_LOAD_NULL,
	INS_ADD,
	INS_SUB,
	INS_MUL,
	INS_DIV,
	INS_MOD,
	INS_POW,
	INS_BITAND,
	INS_BITOR,
	INS_XOR,
	INS_BITNOT,
	INS_SHIFTL,
	INS_SHIFTR,
	INS_AND,
	INS_OR,
	INS_NOT,
	INS_EQUAL,
	INS_NOTEQ,
	INS_LT,
	INS_GT,
	INS_LE,
	INS_GE,
	INS_UPLUS,
	INS_UMINUS,
	INS_IADD,
	INS_ISUB,
	INS_IMUL,
	INS_IDIV,
	INS_IMOD,
	INS_IPOW,
	INS_IBITAND,
	INS_IBITOR,
	INS_IXOR,
	INS_ISHIFTL,
	INS_ISHIFTR,
	INS_IN,
	INS_STORE,
	INS_LOAD,
	INS_LOAD_GLOBAL,
	INS_LOAD_ATTR,
	INS_SET_ATTR,
	INS_LOAD_INDEX,
	INS_SET_INDEX,
	INS_LOAD_NAME,
	INS_PRINT,
	INS_JMP,
	INS_JMP_BACK,
	INS_JMP_IF_TRUE,
	INS_JMP_IF_FALSE,
	INS_JMP_BACK_IF_TRUE,
	INS_JMP_BACK_IF_FALSE,
	INS_JMP_IF_TRUE_ELSE_POP,
	INS_JMP_IF_FALSE_ELSE_POP,
	INS_CALL,
	INS_RETURN,
	INS_THROW,
	INS_TRY_BEGIN,
	INS_TRY_END,
	INS_JMP_IF_EXC_MISMATCH,
	INS_MAKE_LIST,
	INS_MAKE_TUPLE,
	INS_IMPORT,
	INS_EXPORT,
	INS_EXPORT_GLOBAL,
	INS_GET_ITER,
	INS_LOOP_ITER,
	INS_CODEOBJ_INIT,
	INS_POP,
	INS_DUP,
	INS_DUP_TWO,
	INS_ROT,
	INS_ROT_THREE
} Opcode;

typedef enum {
	ST_ENTRY_BEGIN = 0x10,
	ST_ENTRY_END
} STCode;

typedef enum {
	CT_ENTRY_BEGIN = 0x20,
	CT_ENTRY_INT,
	CT_ENTRY_FLOAT,
	CT_ENTRY_STRING,
	CT_ENTRY_CODEOBJ,
	CT_ENTRY_END
} CTCode;

#endif /* OPCODE_H */
