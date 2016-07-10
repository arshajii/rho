#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "object.h"
#include "strobject.h"
#include "util.h"
#include "boolobject.h"

static RhoValue bool_eq(RhoValue *this, RhoValue *other)
{
	if (!rho_isbool(other)) {
		return rho_makefalse();
	}

	return rho_makebool(rho_boolvalue(this) == rho_boolvalue(other));
}

static RhoValue bool_hash(RhoValue *this)
{
	return rho_makeint(rho_util_hash_bool(rho_boolvalue(this)));
}

static RhoValue bool_cmp(RhoValue *this, RhoValue *other)
{
	if (!rho_isbool(other)) {
		return rho_makeut();
	}

	const bool b1 = rho_boolvalue(this);
	const bool b2 = rho_boolvalue(other);
	return rho_makeint((b1 == b2) ? 0 : (b1 ? 1 : -1));
}

static bool bool_nonzero(RhoValue *this)
{
	return rho_boolvalue(this);
}

static RhoValue bool_to_int(RhoValue *this)
{
	return rho_makeint(rho_boolvalue(this) ? 1 : 0);
}

static RhoValue bool_to_float(RhoValue *this)
{
	return rho_makefloat(rho_boolvalue(this) ? 1.0 : 0.0);
}

static RhoValue bool_str(RhoValue *this)
{
	return rho_boolvalue(this) ? rho_strobj_make_direct("true", 4) :
	                             rho_strobj_make_direct("false", 5);
}

struct rho_num_methods rho_bool_num_methods = {
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

	bool_nonzero,    /* nonzero */

	bool_to_int,    /* to_int */
	bool_to_float,    /* to_float */
};

RhoClass rho_bool_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Bool",
	.super = &rho_obj_class,

	.instance_size = 0,

	.init = NULL,
	.del = NULL,

	.eq = bool_eq,
	.hash = bool_hash,
	.cmp = bool_cmp,
	.str = bool_str,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &rho_bool_num_methods,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
