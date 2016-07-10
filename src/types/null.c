#include <stdlib.h>
#include <stdbool.h>
#include "util.h"
#include "object.h"
#include "strobject.h"
#include "null.h"

static RhoValue null_str(RhoValue *this)
{
	RHO_UNUSED(this);
	return rho_strobj_make_direct("null", 4);
}

static RhoValue null_eq(RhoValue *this, RhoValue *other)
{
	RHO_UNUSED(this);
	return rho_makebool(rho_isnull(other));
}

RhoClass rho_null_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Null",
	.super = &rho_obj_class,

	.instance_size = 0,

	.init = NULL,
	.del = NULL,

	.eq = null_eq,
	.hash = NULL,
	.cmp = NULL,
	.str = null_str,
	.call = NULL,

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
