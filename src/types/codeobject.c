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
RhoCodeObject *rho_codeobj_make(RhoCode *code,
                                const char *name,
                                unsigned int argcount,
                                int stack_depth,
                                int try_catch_depth,
                                RhoVM *vm)
{
	RhoCodeObject *co = rho_obj_alloc(&rho_co_class);
	co->name = name;
	co->vm = vm;
	read_lno_table(co, code);
	read_sym_table(co, code);
	read_const_table(co, code);
	co->hints = NULL;
	co->bc = code->bc;
	co->argcount = argcount;
	co->stack_depth = stack_depth;
	co->try_catch_depth = try_catch_depth;
	co->frame = NULL;
	co->cache = rho_calloc(code->size, sizeof(struct rho_code_cache));
	return co;
}

RhoCodeObject *rho_codeobj_make_toplevel(RhoCode *code,
                                         const char *name,
                                         RhoVM *vm)
{
	unsigned int stack_depth = rho_code_read_uint16(code);
	unsigned int try_catch_depth = rho_code_read_uint16(code);
	return rho_codeobj_make(code, name, 0, stack_depth, try_catch_depth, vm);
}

/* last element of `types` should be return value hint */
RhoValue rho_codeobj_init_hints(RhoCodeObject *co, RhoValue *types)
{
	const size_t n_hints = co->argcount + 1;
	co->hints = rho_malloc(n_hints * sizeof(RhoClass *));

	for (size_t i = 0; i < n_hints; i++) {
		if (rho_isnull(&types[i])) {
			co->hints[i] = NULL;
			continue;
		}

		if (rho_getclass(&types[i]) != &rho_meta_class) {
			for (size_t j = 0; j < i; j++) {
				if (&types[j] != NULL) {
					rho_releaseo(&types[j]);
				}
			}

			return RHO_TYPE_EXC("type hint is a %s, not a type", rho_getclass(&types[i])->name);
		}

		RhoClass *type = rho_objvalue(&types[i]);
		rho_retaino(type);
		co->hints[i] = type;
	}

	return rho_makeempty();
}

static void codeobj_free(RhoValue *this)
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

	RhoClass **hints_array = co->hints;
	const size_t hints_size = RHO_CODEOBJ_NUM_HINTS(co);

	for (size_t i = 0; i < hints_size; i++) {
		if (hints_array[i] != NULL) {
			rho_releaseo(hints_array[i]);
		}
	}

	free(hints_array);

	rho_frame_free(co->frame);

	free(co->cache);

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

			constants[i].data.o = rho_codeobj_make(&sub,
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

RhoValue rho_codeobj_load_args(RhoCodeObject *co,
                               struct rho_value_array *default_args,
                               RhoValue *args,
                               RhoValue *args_named,
                               size_t nargs,
                               size_t nargs_named,
                               RhoValue *locals)
{
#define RELEASE_ALL() \
	do { \
		for (unsigned i = 0; i < argcount; i++) \
			if (locals[i].type != RHO_VAL_TYPE_EMPTY) \
				rho_release(&locals[i]); \
	} while (0)

	const unsigned int argcount = co->argcount;

	if (nargs > argcount) {
		return rho_call_exc_num_args(co->name, nargs, argcount);
	}

	for (unsigned i = 0; i < nargs; i++) {
		RhoValue v = args[i];
		rho_retain(&v);
		locals[i] = v;
	}

	struct rho_str_array names = co->names;
	RhoClass **hints = co->hints;

	const unsigned limit = 2*nargs_named;
	for (unsigned i = 0; i < limit; i += 2) {
		RhoStrObject *name = rho_objvalue(&args_named[i]);
		RhoValue v = args_named[i+1];

		bool found = false;
		for (unsigned j = 0; j < argcount; j++) {
			if (strcmp(name->str.value, names.array[j].str) == 0) {
				if (locals[j].type != RHO_VAL_TYPE_EMPTY) {
					RELEASE_ALL();
					return rho_call_exc_dup_arg(co->name, name->str.value);
				}

				if (hints[j] != NULL && !rho_is_a(&v, hints[j])) {
					RELEASE_ALL();
					return rho_type_exc_hint_mismatch(rho_getclass(&v), hints[j]);
				}

				rho_retain(&v);
				locals[j] = v;
				found = true;
				break;
			}
		}

		if (!found) {
			RELEASE_ALL();
			return rho_call_exc_unknown_arg(co->name, name->str.value);
		}
	}

	RhoValue *defaults = default_args->array;
	const unsigned int n_defaults = default_args->length;

	if (defaults == NULL) {
		for (unsigned i = 0; i < argcount; i++) {
			if (locals[i].type == RHO_VAL_TYPE_EMPTY) {
				RELEASE_ALL();
				return rho_call_exc_missing_arg(co->name, names.array[i].str);
			}

			if (hints[i] != NULL && !rho_is_a(&locals[i], hints[i])) {
				RELEASE_ALL();
				return rho_type_exc_hint_mismatch(rho_getclass(&locals[i]), hints[i]);
			}
		}
	} else {
		const unsigned int limit = argcount - n_defaults;  /* where the defaults start */
		for (unsigned i = 0; i < argcount; i++) {
			if (locals[i].type == RHO_VAL_TYPE_EMPTY) {
				if (i >= limit) {
					locals[i] = defaults[i - limit];
					rho_retain(&locals[i]);
				} else {
					RELEASE_ALL();
					return rho_call_exc_missing_arg(co->name, names.array[i].str);
				}
			}

			if (hints[i] != NULL && !rho_is_a(&locals[i], hints[i])) {
				RELEASE_ALL();
				return rho_type_exc_hint_mismatch(rho_getclass(&locals[i]), hints[i]);
			}
		}
	}

	return rho_makeempty();

#undef RELEASE_ALL
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
