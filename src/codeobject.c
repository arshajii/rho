#include <stdlib.h>
#include <assert.h>
#include "code.h"
#include "compiler.h"
#include "opcodes.h"
#include "strobject.h"
#include "err.h"
#include "codeobject.h"

/*
 * The CodeObject bytecode format is similar to the general bytecode format,
 * but with two differences: 1) CodeObject bytecode does not include the
 * "magic" bytes at the start, 2) CodeObject bytecode begins with some
 * metadata describing it.
 *
 *   +-----------------+
 *   | metadata        |
 *   +-----------------+
 *   | line no. table  |
 *   +-----------------+
 *   | symbol table    |
 *   +-----------------+
 *   | constant table  |
 *   +-----------------+
 *   | bytecode        |
 *   +-----------------+
 *
 * Metadata layout:
 *
 *   - Code length in bytes, excluding metadata (uint16)
 *   - Name (null-terminated string)
 *   - Argument count (uint16)
 *   - Value stack size (uint16)
 */

static void read_lno_table(CodeObject *co, Code *code);
static void read_sym_table(CodeObject *co, Code *code);
static void read_const_table(CodeObject *co, Code *code);

/*
 * stack_depth = -1 means that the depth must be read
 * out of `code`.
 */
CodeObject *codeobj_make(Code *code,
                         const char *name,
                         unsigned int argcount,
                         int stack_depth)
{
	CodeObject *co = malloc(sizeof(CodeObject));
	co->base = OBJ_INIT(&co_class);
	co->name = name;
	co->head = code->bc;

	if (stack_depth == -1) {
		stack_depth = code_read_uint16(code);
	} else if (stack_depth < 0) {
		INTERNAL_ERROR();
	}

	read_lno_table(co, code);
	read_sym_table(co, code);
	read_const_table(co, code);
	co->bc = code->bc;
	co->argcount = argcount;
	co->stack_depth = stack_depth;
	return co;
}

void codeobj_free(Value *this)
{
	CodeObject *co = objvalue(this);
	free(co->head);
	free(co->names.array);
	free(co->attrs.array);

	struct value_array *consts = &co->consts;
	Value *consts_array = consts->array;
	const size_t consts_size = consts->length;

	for (size_t i = 0; i < consts_size; i++) {
		release(&consts_array[i]);
	}

	free(consts_array);

	co->base.class->super->del(this);
}

static void read_lno_table(CodeObject *co, Code *code)
{
	const unsigned int first_lineno = code_read_uint16(code);

	const size_t lno_table_size = code_read_uint16(code);
	co->lno_table = code->bc;
	co->first_lineno = first_lineno;

	for (size_t i = 0; i < lno_table_size; i++) {
		code_read_byte(code);
	}
}

static void read_sym_table(CodeObject *co, Code *code)
{
	if (code_read_byte(code) != ST_ENTRY_BEGIN) {
		fatal_error("symbol table expected");
	}

	byte *symtab_bc = code->bc;  /* this is where the table is located */

	while (code_read_byte(code) != ST_ENTRY_END);  /* get past the symbol table */

	size_t off = 0;

	const size_t n_locals = read_uint16_from_stream(symtab_bc);
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

	const size_t n_attrs = read_uint16_from_stream(symtab_bc + off);
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
			constants[i] = strobj_make(STR_INIT(str, str_len, 1));
			break;
		}
		case CT_ENTRY_CODEOBJ: {
			constants[i].type = VAL_TYPE_OBJECT;
			const size_t colen = code_read_uint16(code);
			const char *name = code_read_str(code);
			const unsigned int argcount = code_read_uint16(code);
			const unsigned int stack_depth = code_read_uint16(code);

			Code sub;
			code_init(&sub, colen);
			for (size_t i = 0; i < colen; i++) {
				const byte b = code_read_byte(code);
				code_write_byte(&sub, b);
			}

			constants[i].data.o = codeobj_make(&sub, name, argcount, stack_depth);
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

struct num_methods co_num_methods = {
	NULL,    /* plus */
	NULL,    /* minus */
	NULL,    /* abs */

	NULL,    /* add */
	NULL,    /* sub */
	NULL,    /* mul */
	NULL,    /* div */
	NULL,    /* mod */
	NULL,    /* pow */

	NULL,    /* bitnot */
	NULL,    /* bitand */
	NULL,    /* bitor */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	NULL,    /* iadd */
	NULL,    /* isub */
	NULL,    /* imul */
	NULL,    /* idiv */
	NULL,    /* imod */
	NULL,    /* ipow */

	NULL,    /* ibitand */
	NULL,    /* ibitor */
	NULL,    /* ixor */
	NULL,    /* ishiftl */
	NULL,    /* ishiftr */

	NULL,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct seq_methods co_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* iter */
	NULL,    /* iternext */
};

Class co_class = {
	.name = "CodeObject",

	.super = &obj_class,

	.instance_size = sizeof(CodeObject),

	.init = NULL,
	.del = codeobj_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.num_methods = &co_num_methods,
	.seq_methods = &co_seq_methods,

	.members = NULL,
	.methods = NULL
};
