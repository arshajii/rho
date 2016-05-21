#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "object.h"
#include "strobject.h"
#include "util.h"
#include "intobject.h"

#define TYPE_ERR_STR(op) "invalid operator types for operator " #op "."

#define INT_BINOP_FUNC_BODY(op) \
	if (rho_isint(other)) { \
		return rho_makeint(rho_intvalue(this) op rho_intvalue(other)); \
	} else if (rho_isfloat(other)) { \
		return rho_makefloat(rho_intvalue(this) op rho_floatvalue(other)); \
	} else { \
		return rho_makeut(); \
	}

#define INT_BINOP_FUNC_BODY_NOFLOAT(op) \
	if (rho_isint(other)) { \
		return rho_makeint(rho_intvalue(this) op rho_intvalue(other)); \
	} else { \
		return rho_makeut(); \
	}

#define INT_IBINOP_FUNC_BODY(op) \
	if (rho_isint(other)) { \
		rho_intvalue(this) = rho_intvalue(this) op rho_intvalue(other); \
		return *this; \
	} else if (rho_isfloat(other)) { \
		this->type = RHO_VAL_TYPE_FLOAT; \
		rho_floatvalue(this) = rho_intvalue(this) op rho_floatvalue(other); \
		return *this; \
	} else { \
		return rho_makeut(); \
	}

#define INT_IBINOP_FUNC_BODY_NOFLOAT(op) \
	if (rho_isint(other)) { \
		rho_intvalue(this) = rho_intvalue(this) op rho_intvalue(other); \
		return *this; \
	} else { \
		return rho_makeut(); \
	}

static bool int_eq(RhoValue *this, RhoValue *other)
{
	if (rho_isint(other)) {
		return rho_intvalue(this) == rho_intvalue(other);
	} else if (rho_isfloat(other)) {
		return rho_intvalue(this) == rho_floatvalue(other);
	} else {
		return false;
	}
}

static int int_hash(RhoValue *this)
{
	return rho_util_hash_long(rho_intvalue(this));
}

static RhoValue int_cmp(RhoValue *this, RhoValue *other)
{
	const long x = rho_intvalue(this);
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

static RhoValue int_plus(RhoValue *this)
{
	return *this;
}

static RhoValue int_minus(RhoValue *this)
{
	return rho_makeint(-rho_intvalue(this));
}

static RhoValue int_abs(RhoValue *this)
{
	return rho_makeint(labs(rho_intvalue(this)));
}

static RhoValue int_add(RhoValue *this, RhoValue *other)
{
	INT_BINOP_FUNC_BODY(+)
}

static RhoValue int_sub(RhoValue *this, RhoValue *other)
{
	INT_BINOP_FUNC_BODY(-)
}

static RhoValue int_mul(RhoValue *this, RhoValue *other)
{
	INT_BINOP_FUNC_BODY(*)
}

static RhoValue int_div(RhoValue *this, RhoValue *other)
{
	if (rho_isint(other) && !rho_intvalue(other)) {
		return rho_makedbz();
	}
	INT_BINOP_FUNC_BODY(/)
}

static RhoValue int_mod(RhoValue *this, RhoValue *other)
{
	if (rho_isint(other) && !rho_intvalue(other)) {
		return rho_makedbz();
	}
	INT_BINOP_FUNC_BODY_NOFLOAT(%)
}

static RhoValue int_pow(RhoValue *this, RhoValue *other)
{
	if (rho_isint(other)) {
		return rho_makeint(pow(rho_intvalue(this), rho_intvalue(other)));
	} else if (rho_isfloat(other)) {
		return rho_makefloat(pow(rho_intvalue(this), rho_floatvalue(other)));
	} else {
		return rho_makeut();
	}
}

static RhoValue int_bitnot(RhoValue *this)
{
	return rho_makeint(~rho_intvalue(this));
}

static RhoValue int_bitand(RhoValue *this, RhoValue *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(&)
}

static RhoValue int_bitor(RhoValue *this, RhoValue *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(|)
}

static RhoValue int_xor(RhoValue *this, RhoValue *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(^)
}

static RhoValue int_shiftl(RhoValue *this, RhoValue *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(<<)
}

static RhoValue int_shiftr(RhoValue *this, RhoValue *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(>>)
}

static RhoValue int_iadd(RhoValue *this, RhoValue *other)
{
	INT_IBINOP_FUNC_BODY(+)
}

static RhoValue int_isub(RhoValue *this, RhoValue *other)
{
	INT_IBINOP_FUNC_BODY(-)
}

static RhoValue int_imul(RhoValue *this, RhoValue *other)
{
	INT_IBINOP_FUNC_BODY(*)
}

static RhoValue int_idiv(RhoValue *this, RhoValue *other)
{
	if (rho_isint(other) && !rho_intvalue(other)) {
		return rho_makedbz();
	}
	INT_IBINOP_FUNC_BODY(/)
}

static RhoValue int_imod(RhoValue *this, RhoValue *other)
{
	if (rho_isint(other) && !rho_intvalue(other)) {
		return rho_makedbz();
	}
	INT_IBINOP_FUNC_BODY_NOFLOAT(%)
}

static RhoValue int_ipow(RhoValue *this, RhoValue *other)
{
	if (rho_isint(other)) {
		rho_intvalue(this) = pow(rho_intvalue(this), rho_intvalue(other));
		return *this;
	} else if (rho_isfloat(other)) {
		this->type = RHO_VAL_TYPE_FLOAT;
		rho_floatvalue(this) = pow(rho_intvalue(this), rho_floatvalue(other));
		return *this;
	} else {
		return rho_makeut();
	}
}

static RhoValue int_ibitand(RhoValue *this, RhoValue *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(&)
}

static RhoValue int_ibitor(RhoValue *this, RhoValue *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(|)
}

static RhoValue int_ixor(RhoValue *this, RhoValue *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(^)
}

static RhoValue int_ishiftl(RhoValue *this, RhoValue *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(<<)
}

static RhoValue int_ishiftr(RhoValue *this, RhoValue *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(>>)
}

static bool int_nonzero(RhoValue *this)
{
	return rho_intvalue(this) != 0;
}

static RhoValue int_to_int(RhoValue *this)
{
	return *this;
}

static RhoValue int_to_float(RhoValue *this)
{
	return rho_makefloat(rho_intvalue(this));
}

static RhoStrObject *int_str(RhoValue *this)
{
	char buf[32];
	const long n = rho_intvalue(this);
	int len = snprintf(buf, sizeof(buf), "%ld", n);
	assert(0 < len && (size_t)len < sizeof(buf));

	RhoValue res = rho_strobj_make_direct(buf, len);
	return (RhoStrObject *)rho_objvalue(&res);
}

struct rho_num_methods rho_int_num_methods = {
	int_plus,    /* plus */
	int_minus,    /* minus */
	int_abs,    /* abs */

	int_add,    /* add */
	int_sub,    /* sub */
	int_mul,    /* mul */
	int_div,    /* div */
	int_mod,    /* mod */
	int_pow,    /* pow */

	int_bitnot,    /* bitnot */
	int_bitand,    /* bitand */
	int_bitor,    /* bitor */
	int_xor,    /* xor */
	int_shiftl,    /* shiftl */
	int_shiftr,    /* shiftr */

	int_iadd,    /* iadd */
	int_isub,    /* isub */
	int_imul,    /* imul */
	int_idiv,    /* idiv */
	int_imod,    /* imod */
	int_ipow,    /* ipow */

	int_ibitand,    /* ibitand */
	int_ibitor,    /* ibitor */
	int_ixor,    /* ixor */
	int_ishiftl,    /* ishiftl */
	int_ishiftr,    /* ishiftr */

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

	int_nonzero,    /* nonzero */

	int_to_int,    /* to_int */
	int_to_float,    /* to_float */
};

RhoClass rho_int_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Int",
	.super = &rho_obj_class,

	.instance_size = 0,

	.init = NULL,
	.del = NULL,

	.eq = int_eq,
	.hash = int_hash,
	.cmp = int_cmp,
	.str = int_str,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &rho_int_num_methods,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
