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

static void read_lno_table(CodeObject *co, Code *code);
static void read_sym_table(CodeObject *co, Code *code);
static void read_const_table(CodeObject *co, Code *code);

static void release_defaults(CodeObject *co);

/*
 * stack_depth = -1 means that the depth must be read
 * out of `code`.
 */
CodeObject *codeobj_make(Code *code,
                         const char *name,
                         unsigned int argcount,
                         int stack_depth,
                         int try_catch_depth,
                         VM *vm)
{
	CodeObject *co = obj_alloc(&co_class);
	co->name = name;
	co->head = code->bc;

	if (stack_depth == -1) {
		assert(try_catch_depth == -1);
		stack_depth = code_read_uint16(code);
		try_catch_depth = code_read_uint16(code);
	} else if (stack_depth < 0) {
		INTERNAL_ERROR();
	}

	co->defaults = NULL;
	co->vm = vm;
	co->imported = false;
	read_lno_table(co, code);
	read_sym_table(co, code);
	read_const_table(co, code);
	co->bc = code->bc;
	co->argcount = argcount;
	co->stack_depth = stack_depth;
	co->try_catch_depth = try_catch_depth;
	return co;
}

void codeobj_free(Value *this)
{
	CodeObject *co = objvalue(this);
	free(co->head);
	free(co->names.array);
	free(co->attrs.array);
	free(co->frees.array);
	release_defaults(co);

	struct value_array *consts = &co->consts;
	Value *consts_array = consts->array;
	const size_t consts_size = consts->length;

	for (size_t i = 0; i < consts_size; i++) {
		release(&consts_array[i]);
	}

	free(consts_array);

	obj_class.del(this);
}

void codeobj_init_defaults(CodeObject *co, Value *defaults, const size_t n_defaults)
{
	release_defaults(co);
	co->defaults = rho_malloc(n_defaults * sizeof(Value));
	co->n_defaults = n_defaults;
	for (size_t i = 0; i < n_defaults; i++) {
		co->defaults[i] = defaults[i];
		retain(&defaults[i]);
	}
}

static void release_defaults(CodeObject *co)
{
	Value *defaults = co->defaults;

	if (defaults == NULL) {
		return;
	}

	const unsigned int n_defaults = co->n_defaults;
	for (size_t i = 0; i < n_defaults; i++) {
		release(&defaults[i]);
	}

	free(defaults);
	co->defaults = NULL;
	co->n_defaults = 0;
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
static void read_sym_table(CodeObject *co, Code *code)
{
	assert(code_read_byte(code) == ST_ENTRY_BEGIN);

	byte *symtab_bc = code->bc;  /* this is where the table is located */

	while (code_read_byte(code) != ST_ENTRY_END);  /* get past the symbol table */

	size_t off = 0;

	const size_t n_locals = read_uint16_from_stream(symtab_bc);
	off += 2;

	struct str_array names;
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

	const size_t n_attrs = read_uint16_from_stream(symtab_bc + off);
	off += 2;

	struct str_array attrs;
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

	const size_t n_frees = read_uint16_from_stream(symtab_bc + off);
	off += 2;

	struct str_array frees;
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

static void read_const_table(CodeObject *co, Code *code)
{
	/* read the constant table */
	assert(code_read_byte(code) == CT_ENTRY_BEGIN);

	const size_t ct_size = code_read_uint16(code);
	Value *constants = rho_malloc(ct_size * sizeof(Value));

	for (size_t i = 0; i < ct_size; i++) {
		const byte p = code_read_byte(code);

		switch (p) {
		case CT_ENTRY_BEGIN:
			INTERNAL_ERROR();
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

			char *str = rho_malloc(str_len + 1);
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
			const unsigned int try_catch_depth = code_read_uint16(code);

			Code sub;
			code_init(&sub, colen);
			for (size_t i = 0; i < colen; i++) {
				const byte b = code_read_byte(code);
				code_write_byte(&sub, b);
			}

			constants[i].data.o = codeobj_make(&sub,
			                                   name,
			                                   argcount,
			                                   stack_depth,
			                                   try_catch_depth,
			                                   co->vm);
			break;
		}
		case CT_ENTRY_END:
			INTERNAL_ERROR();
			break;
		default:
			INTERNAL_ERROR();
			break;
		}
	}
	code_read_byte(code);

	co->consts = (struct value_array){.array = constants, .length = ct_size};
}

static Value codeobj_call(Value *this,
                          Value *args,
                          Value *args_named,
                          size_t nargs,
                          size_t nargs_named)
{
#define RELEASE_ALL() \
	do { \
		for (unsigned i = 0; i < argcount; i++) \
			if (locals[i].type != VAL_TYPE_EMPTY) \
				release(&locals[i]); \
		free(locals); \
	} while (0)

	CodeObject *co = objvalue(this);
	VM *vm = co->vm;

	const unsigned int argcount = co->argcount;

	if (nargs > argcount) {
		return call_exc_num_args(co->name, argcount, nargs);
	}

	Value *locals = rho_calloc(argcount, sizeof(Value));

	for (unsigned i = 0; i < nargs; i++) {
		Value v = args[i];
		retain(&v);
		locals[i] = v;
	}

	struct str_array names = co->names;

	const unsigned limit = 2*nargs_named;
	for (unsigned i = 0; i < limit; i += 2) {
		StrObject *name = objvalue(&args_named[i]);
		Value v = args_named[i+1];

		bool found = false;
		for (unsigned j = 0; j < argcount; j++) {
			if (strcmp(name->str.value, names.array[j].str) == 0) {
				if (locals[j].type != VAL_TYPE_EMPTY) {
					RELEASE_ALL();
					return call_exc_dup_arg(co->name, name->str.value);
				}
				retain(&v);
				locals[j] = v;
				found = true;
				break;
			}
		}

		if (!found) {
			RELEASE_ALL();
			return call_exc_unknown_arg(co->name, name->str.value);
		}
	}

	Value *defaults = co->defaults;
	const unsigned int n_defaults = co->n_defaults;

	if (defaults == NULL) {
		for (unsigned i = 0; i < argcount; i++) {
			if (locals[i].type == VAL_TYPE_EMPTY) {
				RELEASE_ALL();
				return call_exc_missing_arg(co->name, names.array[i].str);
			}
		}
	} else {
		const unsigned int limit = argcount - n_defaults;  // where the defaults start
		for (unsigned i = 0; i < argcount; i++) {
			if (locals[i].type == VAL_TYPE_EMPTY) {
				if (i >= limit) {
					locals[i] = defaults[i - limit];
					retain(&locals[i]);
				} else {
					RELEASE_ALL();
					return call_exc_missing_arg(co->name, names.array[i].str);
				}
			}
		}
	}

	retain(this);
	vm_pushframe(vm, co);
	Frame *top = vm->callstack;
	memcpy(top->locals, locals, argcount * sizeof(Value));
	free(locals);
	vm_eval_frame(vm);
	Value res = top->return_value;
	vm_popframe(vm);
	return res;

#undef RELEASE_ALL
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

struct seq_methods co_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
};

Class co_class = {
	.base = CLASS_BASE_INIT(),
	.name = "CodeObject",
	.super = &obj_class,

	.instance_size = sizeof(CodeObject),

	.init = NULL,
	.del = codeobj_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = codeobj_call,

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
