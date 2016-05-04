#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "object.h"
#include "util.h"
#include "exc.h"
#include "object.h"
#include "strobject.h"
#include "metaclass.h"

static void meta_class_del(RhoValue *this)
{
	RHO_UNUSED(this);
}

static RhoStrObject *meta_class_str(RhoValue *this)
{
#define STR_MAX_LEN 50
	char buf[STR_MAX_LEN];
	RhoClass *class = rho_objvalue(this);
	size_t len = snprintf(buf, STR_MAX_LEN, "<class %s>", class->name);
	assert(len > 0);

	if (len > STR_MAX_LEN) {
		len = STR_MAX_LEN;
	}

	RhoValue ret = rho_strobj_make_direct(buf, len);
	return (RhoStrObject *)rho_objvalue(&ret);
#undef STR_MAX_LEN
}

static RhoValue meta_class_call(RhoValue *this,
                             RhoValue *args,
                             RhoValue *args_named,
                             size_t nargs,
                             size_t nargs_named)
{
	RHO_UNUSED(args_named);

	if (nargs_named > 0) {
		return rho_call_exc_constructor_named_args();
	}

	RhoClass *class = rho_objvalue(this);
	InitFunc init = rho_resolve_init(class);

	if (!init) {
		return rho_type_exc_cannot_instantiate(class);
	}

	RhoValue instance = rho_makeobj(rho_obj_alloc(class));
	RhoValue init_result = init(&instance, args, nargs);

	if (rho_iserror(&init_result)) {
		free(rho_objvalue(&instance));  /* straight-up free; no need to
		                               go through `release` since we can
		                               be sure nobody has a reference to
		                               the newly created instance        */
		return init_result;
	} else {
		return instance;
	}
}

RhoClass rho_meta_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "MetaRhoClass",
	.super = &rho_meta_class,

	.instance_size = sizeof(RhoClass),

	.init = NULL,
	.del = meta_class_del,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = meta_class_str,
	.call = meta_class_call,

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
