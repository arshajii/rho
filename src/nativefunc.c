#include <stdlib.h>
#include "object.h"
#include "util.h"
#include "nativefunc.h"

static void nativefunc_free(Value *nfunc)
{
	/*
	 * Purposefully do not call super-destructor,
	 * since NativeFuncObjects should be statically
	 * allocated.
	 */
	UNUSED(nfunc);
}

static Value nativefunc_call(Value *this, Value *args, size_t nargs)
{
	NativeFuncObject *nfunc = objvalue(this);
	return nfunc->func(args, nargs);
}

Class native_func_class = {
	.base = CLASS_BASE_INIT(),
	.name = "NativeFunction",
	.super = &obj_class,

	.instance_size = sizeof(NativeFuncObject),

	.init = NULL,
	.del = nativefunc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = nativefunc_call,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL
};
