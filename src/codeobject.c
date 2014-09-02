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

static void read_sym_table(CodeObject *co, Code *code);
static void read_const_table(CodeObject *co, Code *code);

CodeObject *codeobj_make(Code *code, const char *name, unsigned int argcount)
{
	CodeObject *co = malloc(sizeof(CodeObject));
	co->base = (Object){.class = &co_class, .refcnt = 0};
	co->name = name;
	read_sym_table(co, code);
	read_const_table(co, code);
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

static void read_sym_table(CodeObject *co, Code *code)
{
	if (code_read_byte(code) != ST_ENTRY_BEGIN) {
		fatal_error("symbol table expected");
	}

	byte *symtab_bc = code->bc;  /* this is where the table is located */

	while (code_read_byte(code) != ST_ENTRY_END);  /* get past the symbol table */

	size_t off = 0;

	const size_t n_locals = read_uint16(symtab_bc);
	off += 2;

	struct str_array names;
	names.array = malloc(sizeof(*names.array) * n_locals);
	names.length = n_locals;

	for (size_t i = 0; i < n_locals; i++) {
		size_t len = 0;

		while (symtab_bc[off + len] != '\0') {
			++len;
		}

		assert(len > 0);

		names.array[i].str = (char *)symtab_bc + off;
		names.array[i].length = len;
		off += len + 1;
	}

	const size_t n_attrs = read_uint16(symtab_bc + off);
	off += 2;

	struct str_array attrs;
	attrs.array = malloc(sizeof(*attrs.array) * n_attrs);
	attrs.length = n_attrs;

	for (size_t i = 0; i < n_attrs; i++) {
		size_t len = 0;

		while (symtab_bc[off + len] != '\0') {
			++len;
		}

		assert(len > 0);

		attrs.array[i].str = (char *)symtab_bc + off;
		attrs.array[i].length = len;
		off += len + 1;
	}

	co->names = names;
	co->attrs = attrs;
}

static void read_const_table(CodeObject *co, Code *code)
{
	/* read the constant table */
	if (code_read_byte(code) != CT_ENTRY_BEGIN) {
		fatal_error("constant table expected");
	}

	const size_t ct_size = code_read_uint16(code);
	Value *constants = malloc(ct_size * sizeof(Value));

	for (size_t i = 0; i < ct_size; i++) {
		const byte p = code_read_byte(code);

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

			/*
			 * We read this string manually so we have
			 * access to its length:
			 */
			size_t str_len = 0;

			while (code->bc[str_len] != '\0') {
				++str_len;
			}

			char *str = malloc(str_len + 1);
			for (size_t j = 0; j < str_len; j++) {
				str[j] = code_read_byte(code);
			}
			str[str_len] = '\0';
			code_read_byte(code);  /* skip the string termination byte */
			constants[i] = strobj_make(str_new_direct(str, str_len));
			break;
		}
		case CT_ENTRY_CODEOBJ: {
			constants[i].type = VAL_TYPE_OBJECT;
			size_t colen = code_read_uint16(code);
			const char *name = code_read_str(code);
			unsigned int argcount = code_read_uint16(code);

			Code sub;
			code_init(&sub, colen);
			for (size_t i = 0; i < colen; i++) {
				const byte b = code_read_byte(code);
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
	code_read_byte(code);

	co->consts = (struct value_array){.array = constants, .length = ct_size};
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

	.members = NULL,
	.methods = NULL
};
