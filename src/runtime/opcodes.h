#ifndef RHO_OPCODE_H
#define RHO_OPCODE_H

typedef enum {
	RHO_INS_NOP = 0x30,
	RHO_INS_LOAD_CONST,
	RHO_INS_LOAD_NULL,
	RHO_INS_LOAD_ITER_STOP,
	RHO_INS_ADD,
	RHO_INS_SUB,
	RHO_INS_MUL,
	RHO_INS_DIV,
	RHO_INS_MOD,
	RHO_INS_POW,
	RHO_INS_BITAND,
	RHO_INS_BITOR,
	RHO_INS_XOR,
	RHO_INS_BITNOT,
	RHO_INS_SHIFTL,
	RHO_INS_SHIFTR,
	RHO_INS_AND,
	RHO_INS_OR,
	RHO_INS_NOT,
	RHO_INS_EQUAL,
	RHO_INS_NOTEQ,
	RHO_INS_LT,
	RHO_INS_GT,
	RHO_INS_LE,
	RHO_INS_GE,
	RHO_INS_UPLUS,
	RHO_INS_UMINUS,
	RHO_INS_IADD,
	RHO_INS_ISUB,
	RHO_INS_IMUL,
	RHO_INS_IDIV,
	RHO_INS_IMOD,
	RHO_INS_IPOW,
	RHO_INS_IBITAND,
	RHO_INS_IBITOR,
	RHO_INS_IXOR,
	RHO_INS_ISHIFTL,
	RHO_INS_ISHIFTR,
	RHO_INS_MAKE_RANGE,
	RHO_INS_IN,
	RHO_INS_STORE,
	RHO_INS_STORE_GLOBAL,
	RHO_INS_LOAD,
	RHO_INS_LOAD_GLOBAL,
	RHO_INS_LOAD_ATTR,
	RHO_INS_SET_ATTR,
	RHO_INS_LOAD_INDEX,
	RHO_INS_SET_INDEX,
	RHO_INS_APPLY,
	RHO_INS_IAPPLY,
	RHO_INS_LOAD_NAME,
	RHO_INS_PRINT,
	RHO_INS_JMP,
	RHO_INS_JMP_BACK,
	RHO_INS_JMP_IF_TRUE,
	RHO_INS_JMP_IF_FALSE,
	RHO_INS_JMP_BACK_IF_TRUE,
	RHO_INS_JMP_BACK_IF_FALSE,
	RHO_INS_JMP_IF_TRUE_ELSE_POP,
	RHO_INS_JMP_IF_FALSE_ELSE_POP,
	RHO_INS_CALL,
	RHO_INS_RETURN,
	RHO_INS_THROW,
	RHO_INS_PRODUCE,
	RHO_INS_TRY_BEGIN,
	RHO_INS_TRY_END,
	RHO_INS_JMP_IF_EXC_MISMATCH,
	RHO_INS_MAKE_LIST,
	RHO_INS_MAKE_TUPLE,
	RHO_INS_MAKE_SET,
	RHO_INS_MAKE_DICT,
	RHO_INS_IMPORT,
	RHO_INS_EXPORT,
	RHO_INS_EXPORT_GLOBAL,
	RHO_INS_EXPORT_NAME,
	RHO_INS_RECEIVE,
	RHO_INS_GET_ITER,
	RHO_INS_LOOP_ITER,
	RHO_INS_MAKE_FUNCOBJ,
	RHO_INS_MAKE_GENERATOR,
	RHO_INS_MAKE_ACTOR,
	RHO_INS_SEQ_EXPAND,
	RHO_INS_POP,
	RHO_INS_DUP,
	RHO_INS_DUP_TWO,
	RHO_INS_ROT,
	RHO_INS_ROT_THREE
} RhoOpcode;

typedef enum {
	RHO_ST_ENTRY_BEGIN = 0x10,
	RHO_ST_ENTRY_END
} RhoSTCode;

typedef enum {
	RHO_CT_ENTRY_BEGIN = 0x20,
	RHO_CT_ENTRY_INT,
	RHO_CT_ENTRY_FLOAT,
	RHO_CT_ENTRY_STRING,
	RHO_CT_ENTRY_CODEOBJ,
	RHO_CT_ENTRY_END
} RhoCTCode;

#endif /* RHO_OPCODE_H */
