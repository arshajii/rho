#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "code.h"
#include "compiler.h"
#include "opcodes.h"
#include "object.h"
#include "strobject.h"
#include "vm.h"
#include "util.h"
#include "exc.h"
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

static void read_lno_table(RhoCodeObject *co, RhoCode *code);
static void read_sym_table(RhoCodeObject *co, RhoCode *code);
static void read_const_table(RhoCodeObject *co, RhoCode *code);

/*
 * stack_depth = -1 means that the depth must be read
 * out of `code`.
 */
RhoCodeObject *codeobj_make(RhoCode *code,
                         const char *name,
                         unsigned int argcount,
                         int stack_depth,
                         int try_catch_depth,
                         RhoVM *vm)
{
	RhoCodeObject *co = rho_obj_alloc(&rho_co_class);
	co->name = name;

	if (stack_depth == -1) {
		assert(try_catch_depth == -1);
		stack_depth = rho_code_read_uint16(code);
		try_catch_depth = rho_code_read_uint16(code);
	} else if (stack_depth < 0) {
		RHO_INTERNAL_ERROR();
	}

	co->vm = vm;
	read_lno_table(co, code);
	read_sym_table(co, code);
	read_const_table(co, code);
	co->bc = code->bc;
	co->argcount = argcount;
	co->stack_depth = stack_depth;
	co->try_catch_depth = try_catch_depth;
	return co;
}

void codeobj_free(RhoValue *this)
{
	RhoCodeObject *co = rho_objvalue(this);
	free(co->names.array);
	free(co->attrs.array);
	free(co->frees.array);

	struct rho_value_array *consts = &co->consts;
	RhoValue *consts_array = consts->array;
	const size_t consts_size = consts->length;

	for (size_t i = 0; i < consts_size; i++) {
		rho_release(&consts_array[i]);
	}

	free(consts_array);

	rho_obj_class.del(this);
}

static void read_lno_table(RhoCodeObject *co, RhoCode *code)
{
	const unsigned int first_lineno = rho_code_read_uint16(code);

	const size_t lno_table_size = rho_code_read_uint16(code);
	co->lno_table = code->bc;
	co->first_lineno = first_lineno;

	for (size_t i = 0; i < lno_table_size; i++) {
		rho_code_read_byte(code);
	}
}

/*
 * Symbol table has 3 components:
 *
 *   - Table of local (bounded) variable names
 *   - Table of attributes
 *   - Table of free (unbounded) variable names
 *
 * "Table" in this context refers to the following format:
 *
 *   - 2 bytes: number of table entries (N)
 *   - N null-terminated strings
 *
 * Example table: 2 0 'f' 'o' 'o' 0 'b' 'a' 'r' 0
 */
static void read_sym_table(RhoCodeObject *co, RhoCode *code)
{
	assert(rho_code_read_byte(code) == RHO_ST_ENTRY_BEGIN);

	byte *symtab_bc = code->bc;  /* this is where the table is located */

	while (rho_code_read_byte(code) != RHO_ST_ENTRY_END);  /* get past the symbol table */

	size_t off = 0;

	const size_t n_locals = rho_util_read_uint16_from_stream(symtab_bc);
	off += 2;

	struct rho_str_array names;
	names.array = rho_malloc(sizeof(*names.array) * n_locals);
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

	const size_t n_attrs = rho_util_read_uint16_from_stream(symtab_bc + off);
	off += 2;

	struct rho_str_array attrs;
	attrs.array = rho_malloc(sizeof(*attrs.array) * n_attrs);
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

	const size_t n_frees = rho_util_read_uint16_from_stream(symtab_bc + off);
	off += 2;

	struct rho_str_array frees;
	frees.array = rho_malloc(sizeof(*frees.array) * n_frees);
	frees.length = n_frees;

	for (size_t i = 0; i < n_frees; i++) {
		size_t len = 0;

		while (symtab_bc[off + len] != '\0') {
			++len;
		}

		assert(len > 0);

		frees.array[i].str = (char *)symtab_bc + off;
		frees.array[i].length = len;
		off += len + 1;
	}

	co->names = names;
	co->attrs = attrs;
	co->frees = frees;
}

static void read_const_table(RhoCodeObject *co, RhoCode *code)
{
	/* read the constant table */
	assert(rho_code_read_byte(code) == RHO_CT_ENTRY_BEGIN);

	const size_t ct_size = rho_code_read_uint16(code);
	RhoValue *constants = rho_malloc(ct_size * sizeof(RhoValue));

	for (size_t i = 0; i < ct_size; i++) {
		const byte p = rho_code_read_byte(code);

		switch (p) {
		case RHO_CT_ENTRY_BEGIN:
			RHO_INTERNAL_ERROR();
			break;
		case RHO_CT_ENTRY_INT:
			constants[i].type = RHO_VAL_TYPE_INT;
			constants[i].data.i = rho_code_read_int(code);
			break;
		case RHO_CT_ENTRY_FLOAT:
			constants[i].type = RHO_VAL_TYPE_FLOAT;
			constants[i].data.f = rho_code_read_double(code);
			break;
		case RHO_CT_ENTRY_STRING: {
			constants[i].type = RHO_VAL_TYPE_OBJECT;

			/*
			 * We read this string manually so we have
			 * access to its length:
			 */
			size_t str_len = 0;

			while (code->bc[str_len] != '\0') {
				++str_len;
			}

			char *str = rho_malloc(str_len + 1);
			for (size_t j = 0; j < str_len; j++) {
				str[j] = rho_code_read_byte(code);
			}
			str[str_len] = '\0';
			rho_code_read_byte(code);  /* skip the string termination byte */
			constants[i] = rho_strobj_make(RHO_STR_INIT(str, str_len, 1));
			break;
		}
		case RHO_CT_ENTRY_CODEOBJ: {
			constants[i].type = RHO_VAL_TYPE_OBJECT;
			const size_t code_len = rho_code_read_uint16(code);
			const char *name = rho_code_read_str(code);
			const unsigned int argcount = rho_code_read_uint16(code);
			const unsigned int stack_depth = rho_code_read_uint16(code);
			const unsigned int try_catch_depth = rho_code_read_uint16(code);

			RhoCode sub;
			sub.bc = code->bc;
			sub.size = code_len;
			sub.capacity = 0;
			rho_code_skip_ahead(code, code_len);

			constants[i].data.o = codeobj_make(&sub,
			                                   name,
			                                   argcount,
			                                   stack_depth,
			                                   try_catch_depth,
			                                   co->vm);
			break;
		}
		case RHO_CT_ENTRY_END:
			RHO_INTERNAL_ERROR();
			break;
		default:
			RHO_INTERNAL_ERROR();
			break;
		}
	}
	assert(rho_code_read_byte(code) == RHO_CT_ENTRY_END);

	co->consts = (struct rho_value_array){.array = constants, .length = ct_size};
}

struct rho_num_methods co_num_methods = {
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

	NULL,    /* radd */
	NULL,    /* rsub */
	NULL,    /* rmul */
	NULL,    /* rdiv */
	NULL,    /* rmod */
	NULL,    /* rpow */

	NULL,    /* rbitand */
	NULL,    /* rbitor */
	NULL,    /* rxor */
	NULL,    /* rshiftl */
	NULL,    /* rshiftr */

	NULL,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct rho_seq_methods co_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_co_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "CodeObject",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoCodeObject),

	.init = NULL,
	.del = codeobj_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &co_num_methods,
	.seq_methods = &co_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
