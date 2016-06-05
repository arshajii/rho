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

static void release_defaults(RhoFuncObject *fn);

RhoValue rho_funcobj_make(RhoCodeObject *co)
{
	RhoFuncObject *fn = rho_obj_alloc(&rho_fn_class);
	rho_retaino(co);
	fn->co = co;
	fn->defaults = (struct rho_value_array){.array = NULL, .length = 0};
	return rho_makeobj(fn);
}

void funcobj_free(RhoValue *this)
{
	RhoFuncObject *fn = rho_objvalue(this);
	release_defaults(fn);
	rho_releaseo(fn->co);
	rho_obj_class.del(this);
}

void rho_funcobj_init_defaults(RhoFuncObject *fn, RhoValue *defaults, const size_t n_defaults)
{
	release_defaults(fn);
	fn->defaults.array = rho_malloc(n_defaults * sizeof(RhoValue));
	fn->defaults.length = n_defaults;
	for (size_t i = 0; i < n_defaults; i++) {
		fn->defaults.array[i] = defaults[i];
		rho_retain(&defaults[i]);
	}
}

static void release_defaults(RhoFuncObject *fn)
{
	RhoValue *defaults = fn->defaults.array;

	if (defaults == NULL) {
		return;
	}

	const unsigned int n_defaults = fn->defaults.length;
	for (size_t i = 0; i < n_defaults; i++) {
		rho_release(&defaults[i]);
	}

	free(defaults);
	fn->defaults = (struct rho_value_array){.array = NULL, .length = 0};
}

static RhoValue funcobj_call(RhoValue *this,
                             RhoValue *args,
                             RhoValue *args_named,
                             size_t nargs,
                             size_t nargs_named)
{
	RhoFuncObject *fn = rho_objvalue(this);
	RhoCodeObject *co = fn->co;
	RhoVM *vm = rho_current_vm_get();
	const unsigned int argcount = co->argcount;

	RhoValue *locals = rho_calloc(argcount, sizeof(RhoValue));
	RhoValue status = rho_codeobj_load_args(co, &fn->defaults, args, args_named, nargs, nargs_named, locals);

	if (rho_iserror(&status)) {
		free(locals);
		return status;
	}

	rho_retaino(co);
	rho_vm_push_frame(vm, co);
	RhoFrame *top = vm->callstack;
	memcpy(top->locals, locals, argcount * sizeof(RhoValue));
	free(locals);
	rho_vm_eval_frame(vm);
	RhoValue res = top->return_value;
	rho_vm_pop_frame(vm);
	return res;

#undef RELEASE_ALL
}

struct rho_num_methods fn_num_methods = {
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

struct rho_seq_methods fn_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_fn_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "FuncObject",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoFuncObject),

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
