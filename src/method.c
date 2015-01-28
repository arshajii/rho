#include <stdlib.h>
#include "object.h"
#include "method.h"

Value methobj_make(Object *binder, MethodFunc meth_func)
{
	Method *meth = obj_alloc(&method_class);
	retaino(binder);
	meth->binder = binder;
	meth->method = meth_func;
	return makeobj(meth);
}

static void methobj_free(Value *this)
{
	Method *meth = objvalue(this);
	releaseo(meth->binder);
	meth->base.class->super->del(this);
}

static Value methobj_invoke(Value *this, Value *args, size_t nargs)
{
	Method *meth = objvalue(this);
	return meth->method(&makeobj(meth->binder), args, nargs);
}

struct num_methods meth_num_methods = {
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

struct seq_methods meth_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
};

Class method_class = {
	.name = "Method",

	.super = &obj_class,

	.instance_size = sizeof(Method),

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
	.seq_methods  = &meth_seq_methods,

	.members = NULL,
	.methods = NULL
};
