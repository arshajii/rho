#include <stdlib.h>
#include "object.h"
#include "method.h"

RhoValue rho_methobj_make(RhoValue *binder, MethodFunc meth_func)
{
	RhoMethod *meth = rho_obj_alloc(&rho_method_class);
	rho_retain(binder);
	meth->binder = *binder;
	meth->method = meth_func;
	return rho_makeobj(meth);
}

static void methobj_free(RhoValue *this)
{
	RhoMethod *meth = rho_objvalue(this);
	rho_release(&meth->binder);
	rho_obj_class.del(this);
}

static RhoValue methobj_invoke(RhoValue *this,
                               RhoValue *args,
                               RhoValue *args_named,
                               size_t nargs,
                               size_t nargs_named)
{
	RhoMethod *meth = rho_objvalue(this);
	return meth->method(&meth->binder, args, args_named, nargs, nargs_named);
}

struct rho_num_methods meth_num_methods = {
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

struct rho_seq_methods meth_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_method_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Method",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoMethod),

	.init = NULL,
	.del = methobj_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = methobj_invoke,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &meth_num_methods,
	.seq_methods = &meth_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
