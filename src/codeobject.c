#include <stdlib.h>
#include <assert.h>
#include "code.h"
#include "compiler.h"
#include "opcodes.h"
#include "strobject.h"
#include "err.h"
#include "codeobject.h"

static struct str_array read_sym_table(Code *code);
static struct value_array read_const_table(Code *code);

CodeObject *codeobj_make(Code *code)
{
	// TODO: co.argcount ?
	CodeObject *co = malloc(sizeof(CodeObject));
	co->base = (Object){.class = &co_class, .refcnt = 0};
	co->names = read_sym_table(code);
	co->consts = read_const_table(code);
	co->bc = code->bc;
	co->argcount = 0;  // for now
	return co;
}

void codeobj_free(Value *this)
{
	CodeObject *co = this->data.o;
	free(co->bc);
	free(co->names.array);
	free(co->consts.array);
	free(co);
	co->base.class->super->del(this);
}

static struct str_array read_sym_table(Code *code)
{
	if (code_fwd(code) != ST_ENTRY_BEGIN) {
		fatal_error("symbol table expected");
	}

	byte *symtab_bc = code->bc;  /* this is where the table is located */

	while (code_fwd(code) != ST_ENTRY_END);  /* get past the symbol table */

	unsigned int count = 0;
	unsigned int off = 0;

	const size_t symtab_len = read_int(symtab_bc);
	off += INT_SIZE;

	struct str_array table;
	table.array = malloc(sizeof(*table.array) * symtab_len);
	table.length = symtab_len;

	while (symtab_bc[off] != ST_ENTRY_END) {
		size_t len = 0;

		while (symtab_bc[off + len] != '\0') {
			++len;
		}

		assert(len > 0);

		table.array[count].str = (char *)symtab_bc + off;
		table.array[count].length = len;
		++count;
		off += len + 1;
	}

	return table;
}

static struct value_array read_const_table(Code *code)
{
	/* read the constant table */
	if (code_fwd(code) != CT_ENTRY_BEGIN) {
		fatal_error("constant table expected");
	}

	const size_t ct_size = code_read_int(code);
	Value *constants = malloc(ct_size * sizeof(Value));

	for (size_t i = 0; i < ct_size; i++) {
		const byte p = code_fwd(code);

		switch (p) {
		case CT_ENTRY_BEGIN:
			fatal_error("unexpected CT_ENTRY_BEGIN in constant table");
			break;
		case CT_ENTRY_INT:
			constants[i].type = VAL_TYPE_INT;
			constants[i].data.i = code_read_int(code);
			break;
		case CT_ENTRY_FLOAT:
			constants[i].type = VAL_TYPE_FLOAT;
			constants[i].data.f = code_read_double(code);
			break;
		case CT_ENTRY_STRING: {
			constants[i].type = VAL_TYPE_OBJECT;
			size_t strlen = 0;

			while (*(code->bc + strlen) != '\0') {
				++strlen;
			}

			char *str = malloc(strlen + 1);
			for (size_t j = 0; j < strlen; j++) {
				str[j] = code_fwd(code);
			}
			str[strlen] = '\0';
			constants[i] = strobj_make(str_new_direct(str, strlen));
			code_fwd(code);  /* skip the string termination byte */
			break;
		}
		case CT_ENTRY_CODEOBJ: {
			constants[i].type = VAL_TYPE_OBJECT;
			size_t colen = code_read_int(code);
			Code sub;
			code_init(&sub, colen);
			for (size_t i = 0; i < colen; i++) {
				code_write_byte(&sub, code_fwd(code));
			}
			constants[i].data.o = codeobj_make(&sub);
			break;
		}
		case CT_ENTRY_END:
			fatal_error("unexpected CT_ENTRY_END in constant table");
			break;
		default:
			UNEXP_BYTE(p);
			break;
		}
	}
	code_fwd(code);

	return (struct value_array){.array = constants, .length = ct_size};
}

Class co_class = {
	.name = "CodeObject",

	.super = &obj_class,

	.new = NULL,
	.del = codeobj_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,
};
