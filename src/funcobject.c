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
#include "funcobject.h"

static void release_defaults(FuncObject *fn);

Value funcobj_make(CodeObject *co)
{
	FuncObject *fn = obj_alloc(&fn_class);
	retaino(co);
	fn->co = co;
	fn->defaults = (struct value_array){.array = NULL, .length = 0};
	return makeobj(fn);
}

void funcobj_free(Value *this)
{
	FuncObject *fn = objvalue(this);
	release_defaults(fn);
	releaseo(fn->co);
	obj_class.del(this);
}

void funcobj_init_defaults(FuncObject *fn, Value *defaults, const size_t n_defaults)
{
	release_defaults(fn);
	fn->defaults.array = rho_malloc(n_defaults * sizeof(Value));
	fn->defaults.length = n_defaults;
	for (size_t i = 0; i < n_defaults; i++) {
		fn->defaults.array[i] = defaults[i];
		retain(&defaults[i]);
	}
}

static void release_defaults(FuncObject *fn)
{
	Value *defaults = fn->defaults.array;

	if (defaults == NULL) {
		return;
	}

	const unsigned int n_defaults = fn->defaults.length;
	for (size_t i = 0; i < n_defaults; i++) {
		release(&defaults[i]);
	}

	free(defaults);
	fn->defaults = (struct value_array){.array = NULL, .length = 0};
}

static Value funcobj_call(Value *this,
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

	FuncObject *fn = objvalue(this);
	CodeObject *co = fn->co;
	VM *vm = get_current_vm();

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

	Value *defaults = fn->defaults.array;
	const unsigned int n_defaults = fn->defaults.length;

	if (defaults == NULL) {
		for (unsigned i = 0; i < argcount; i++) {
			if (locals[i].type == VAL_TYPE_EMPTY) {
				RELEASE_ALL();
				return call_exc_missing_arg(co->name, names.array[i].str);
			}
		}
	} else {
		const unsigned int limit = argcount - n_defaults;  /* where the defaults start */
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

	retaino(co);
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

struct num_methods fn_num_methods = {
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

struct seq_methods fn_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

Class fn_class = {
	.base = CLASS_BASE_INIT(),
	.name = "FuncObject",
	.super = &obj_class,

	.instance_size = sizeof(FuncObject),

	.init = NULL,
	.del = funcobj_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = funcobj_call,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &fn_num_methods,
	.seq_methods = &fn_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
