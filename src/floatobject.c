#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "object.h"
#include "strobject.h"
#include "util.h"
#include "floatobject.h"

#define TYPE_ERR_STR(op) "invalid operator types for operator " #op "."

#define FLOAT_BINOP_FUNC_BODY(op) \
	if (rho_isint(other)) { \
		return rho_makefloat(rho_floatvalue(this) op rho_intvalue(other)); \
	} else if (rho_isfloat(other)) { \
		return rho_makefloat(rho_floatvalue(this) op rho_floatvalue(other)); \
	} else { \
		return rho_makeut(); \
	}

#define FLOAT_IBINOP_FUNC_BODY(op) \
	if (rho_isint(other)) { \
		rho_floatvalue(this) = rho_floatvalue(this) op rho_intvalue(other); \
		return *this; \
	} else if (rho_isfloat(other)) { \
		rho_floatvalue(this) = rho_floatvalue(this) op rho_floatvalue(other); \
		return *this; \
	} else { \
		return rho_makeut(); \
	}

static bool float_eq(RhoValue *this, RhoValue *other)
{
	if (rho_isint(other)) {
		return rho_floatvalue(this) == rho_intvalue(other);
	} else if (rho_isfloat(other)) {
		return rho_floatvalue(this) == rho_floatvalue(other);
	} else {
		return false;
	}
}

static int float_hash(RhoValue *this)
{
	return rho_util_hash_double(rho_floatvalue(this));
}

static RhoValue float_cmp(RhoValue *this, RhoValue *other)
{
	const double x = rho_floatvalue(this);
	if (rho_isint(other)) {
		const long y = rho_intvalue(other);
		return rho_makeint((x < y) ? -1 : ((x == y) ? 0 : 1));
	} else if (rho_isfloat(other)) {
		const double y = rho_floatvalue(other);
		return rho_makeint((x < y) ? -1 : ((x == y) ? 0 : 1));
	} else {
		return rho_makeut();
	}
}

static RhoValue float_plus(RhoValue *this)
{
	return *this;
}

static RhoValue float_minus(RhoValue *this)
{
	return rho_makefloat(-rho_floatvalue(this));
}

static RhoValue float_abs(RhoValue *this)
{
	return rho_makefloat(fabs(rho_floatvalue(this)));
}

static RhoValue float_add(RhoValue *this, RhoValue *other)
{
	FLOAT_BINOP_FUNC_BODY(+)
}

static RhoValue float_sub(RhoValue *this, RhoValue *other)
{
	FLOAT_BINOP_FUNC_BODY(-)
}

static RhoValue float_mul(RhoValue *this, RhoValue *other)
{
	FLOAT_BINOP_FUNC_BODY(*)
}

static RhoValue float_div(RhoValue *this, RhoValue *other)
{
	FLOAT_BINOP_FUNC_BODY(/)
}

static RhoValue float_pow(RhoValue *this, RhoValue *other)
{
	if (rho_isint(other)) {
		return rho_makefloat(pow(rho_floatvalue(this), rho_intvalue(other)));
	} else if (rho_isfloat(other)) {
		return rho_makefloat(pow(rho_floatvalue(this), rho_floatvalue(other)));
	} else {
		return rho_makeut();
	}
}

static RhoValue float_iadd(RhoValue *this, RhoValue *other)
{
	FLOAT_IBINOP_FUNC_BODY(+)
}

static RhoValue float_isub(RhoValue *this, RhoValue *other)
{
	FLOAT_IBINOP_FUNC_BODY(-)
}

static RhoValue float_imul(RhoValue *this, RhoValue *other)
{
	FLOAT_IBINOP_FUNC_BODY(*)
}

static RhoValue float_idiv(RhoValue *this, RhoValue *other)
{
	FLOAT_IBINOP_FUNC_BODY(/)
}

static RhoValue float_ipow(RhoValue *this, RhoValue *other)
{
	if (rho_isint(other)) {
		rho_floatvalue(this) = pow(rho_floatvalue(this), rho_intvalue(other));
		return *this;
	} else if (rho_isfloat(other)) {
		rho_floatvalue(this) = pow(rho_floatvalue(this), rho_floatvalue(other));
		return *this;
	} else {
		return rho_makeut();
	}
}

static bool float_nonzero(RhoValue *this)
{
	return rho_floatvalue(this) != 0;
}

static RhoValue float_to_int(RhoValue *this)
{
	return rho_makeint(rho_floatvalue(this));
}

static RhoValue float_to_float(RhoValue *this)
{
	return *this;
}

static RhoStrObject *float_str(RhoValue *this)
{
	char buf[32];
	const double d = rho_floatvalue(this);
	int len = snprintf(buf, sizeof(buf), "%f", d);
	assert(0 < len && (size_t)len < sizeof(buf));

	RhoValue res = rho_strobj_make_direct(buf, len);
	return (RhoStrObject *)rho_objvalue(&res);
}

struct rho_num_methods rho_float_num_methods = {
	float_plus,    /* plus */
	float_minus,    /* minus */
	float_abs,    /* abs */

	float_add,    /* add */
	float_sub,    /* sub */
	float_mul,    /* mul */
	float_div,    /* div */
	NULL,    /* mod */
	float_pow,    /* pow */

	NULL,    /* bitnot */
	NULL,    /* bitand */
	NULL,    /* bitor */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	float_iadd,    /* iadd */
	float_isub,    /* isub */
	float_imul,    /* imul */
	float_idiv,    /* idiv */
	NULL,    /* imod */
	float_ipow,    /* ipow */

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

	float_nonzero,    /* nonzero */

	float_to_int,    /* to_int */
	float_to_float,    /* to_float */
};

RhoClass rho_float_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Float",
	.super = &rho_obj_class,

	.instance_size = 0,

	.init = NULL,
	.del = NULL,

	.eq = float_eq,
	.hash = float_hash,
	.cmp = float_cmp,
	.str = float_str,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &rho_float_num_methods,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
