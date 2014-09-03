#include <stdlib.h>
#include "object.h"
#include "method.h"

Value methobj_make(Object *binder, MethodFunc meth_func)
{
	Method *meth = malloc(sizeof(Method));
	meth->base = (Object){.class = &method_class, .refcnt = 0};
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

Class method_class = {
	.name = "Method",

	.super = &obj_class,

	.new = NULL,
	.del = methobj_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = methobj_invoke,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL
};
