#include <stdlib.h>
#include <assert.h>
#include "code.h"
#include "compiler.h"
#include "opcodes.h"
#include "strobject.h"
#include "err.h"
#include "codeobject.h"

/*
 * The CodeObject bytecode format is the same as the general bytecode format.
 * Note, however, that CodeObject bytecode does not include the magic bytes
 * at the start.
 *
 * +-----------------+
 * | symbol table    |
 * +-----------------+
 * | constant table  |
 * +-----------------+
 * | bytecode        |
 * +-----------------+
 *
 * When a CodeObject is saved in a constant table and the written to the
 * bytecode, that CodeObject's code should start with a null-terminated
 * string representing the CodeObject's name, followed by a 4-byte int
 * indicating how many arguments the CodeObject takes. The symbol table
 * of a CodeObject should start with its arguments names (in order).
 */

static struct str_array read_sym_table(Code *code);
static struct value_array read_const_table(Code *code);

CodeObject *codeobj_make(Code *code, const char *name, int argcount)
{
	CodeObject *co = malloc(sizeof(CodeObject));
	co->base = (Object){.class = &co_class, .refcnt = 0};
	co->name = name;
	co->names = read_sym_table(code);
	co->consts = read_const_table(code);
	co->bc = code->bc;
	co->argcount = argcount;
	return co;
}

void codeobj_free(Value *this)
{
	CodeObject *co = this->data.o;
	free(co->bc);
	free((char *)co->name);
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
			size_t str_len = 0;

			while (code->bc[str_len] != '\0') {
				++str_len;
			}

			char *str = malloc(str_len + 1);
			for (size_t j = 0; j < str_len; j++) {
				str[j] = code_fwd(code);
			}
			str[str_len] = '\0';
			code_fwd(code);  /* skip the string termination byte */
			constants[i] = strobj_make(str_new_direct(str, str_len));
			break;
		}
		case CT_ENTRY_CODEOBJ: {
			constants[i].type = VAL_TYPE_OBJECT;
			size_t colen = code_read_int(code);

			size_t name_len = 0;

			while (code->bc[name_len] != '\0') {
				++name_len;
			}

			char *name = malloc(name_len + 1);
			for (size_t j = 0; j < name_len; j++) {
				name[j] = code_fwd(code);
			}
			code_fwd(code);  /* skip the string termination byte */
			name[name_len] = '\0';

			int argcount = code_read_int(code);

			Code sub;
			code_init(&sub, colen);
			for (size_t i = 0; i < colen; i++) {
				byte b = code_fwd(code);
				code_write_byte(&sub, b);
			}

			constants[i].data.o = codeobj_make(&sub, name, argcount);
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
