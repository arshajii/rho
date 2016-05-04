#include <stdlib.h>
#include "object.h"
#include "util.h"
#include "exc.h"
#include "nativefunc.h"

static void nativefunc_free(RhoValue *nfunc)
{
	/*
	 * Purposefully do not call super-destructor,
	 * since NativeFuncObjects should be statically
	 * allocated.
	 */
	RHO_UNUSED(nfunc);
}

static RhoValue nativefunc_call(RhoValue *this,
                             RhoValue *args,
                             RhoValue *args_named,
                             size_t nargs,
                             size_t nargs_named)
{
	RHO_UNUSED(args_named);

	if (nargs_named > 0) {
		return rho_call_exc_native_named_args();
	}

	RhoNativeFuncObject *nfunc = rho_objvalue(this);
	return nfunc->func(args, nargs);
}

RhoClass rho_native_func_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "NativeFunction",
	.super = &obj_class,

	.instance_size = sizeof(RhoNativeFuncObject),

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
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
