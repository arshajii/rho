#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "object.h"
#include "str.h"
#include "util.h"
#include "intobject.h"

#define TYPE_ERR_STR(op) "invalid operator types for operator " #op "."

#define INT_BINOP_FUNC_BODY(op) \
	if (isint(other)) { \
		return makeint(intvalue(this) op intvalue(other)); \
	} else if (isfloat(other)) { \
		return makefloat(intvalue(this) op floatvalue(other)); \
	} else { \
		return makeut(); \
	}

#define INT_BINOP_FUNC_BODY_NOFLOAT(op) \
	if (isint(other)) { \
		return makeint(intvalue(this) op intvalue(other)); \
	} else { \
		return makeut(); \
	}

#define INT_IBINOP_FUNC_BODY(op) \
	if (isint(other)) { \
		intvalue(this) = intvalue(this) op intvalue(other); \
		return *this; \
	} else if (isfloat(other)) { \
		this->type = VAL_TYPE_FLOAT; \
		floatvalue(this) = intvalue(this) op floatvalue(other); \
		return *this; \
	} else { \
		return makeut(); \
	}

#define INT_IBINOP_FUNC_BODY_NOFLOAT(op) \
	if (isint(other)) { \
		intvalue(this) = intvalue(this) op intvalue(other); \
		return *this; \
	} else { \
		return makeut(); \
	}

static bool int_eq(Value *this, Value *other)
{
	if (isint(other)) {
		return intvalue(this) == intvalue(other);
	} else if (isfloat(other)) {
		return intvalue(this) == floatvalue(other);
	} else {
		return false;
	}
}

static int int_hash(Value *this)
{
	return hash_long(intvalue(this));
}

static Value int_cmp(Value *this, Value *other)
{
	const long x = intvalue(this);
	if (isint(other)) {
		const long y = intvalue(other);
		return makeint((x < y) ? -1 : ((x == y) ? 0 : 1));
	} else if (isfloat(other)) {
		const double y = floatvalue(other);
		return makeint((x < y) ? -1 : ((x == y) ? 0 : 1));
	} else {
		return makeut();
	}
}

static Value int_plus(Value *this)
{
	return *this;
}

static Value int_minus(Value *this)
{
	return makeint(-intvalue(this));
}

static Value int_abs(Value *this)
{
	return makeint(labs(intvalue(this)));
}

static Value int_add(Value *this, Value *other)
{
	INT_BINOP_FUNC_BODY(+)
}

static Value int_sub(Value *this, Value *other)
{
	INT_BINOP_FUNC_BODY(-)
}

static Value int_mul(Value *this, Value *other)
{
	INT_BINOP_FUNC_BODY(*)
}

static Value int_div(Value *this, Value *other)
{
	if (isint(other) && !intvalue(other)) {
		return makedbz();
	}
	INT_BINOP_FUNC_BODY(/)
}

static Value int_mod(Value *this, Value *other)
{
	if (isint(other) && !intvalue(other)) {
		return makedbz();
	}
	INT_BINOP_FUNC_BODY_NOFLOAT(%)
}

static Value int_pow(Value *this, Value *other)
{
	if (isint(other)) {
		return makeint(pow(intvalue(this), intvalue(other)));
	} else if (isfloat(other)) {
		return makefloat(pow(intvalue(this), floatvalue(other)));
	} else {
		return makeut();
	}
}

static Value int_bitnot(Value *this)
{
	return makeint(~intvalue(this));
}

static Value int_bitand(Value *this, Value *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(&)
}

static Value int_bitor(Value *this, Value *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(|)
}

static Value int_xor(Value *this, Value *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(^)
}

static Value int_shiftl(Value *this, Value *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(<<)
}

static Value int_shiftr(Value *this, Value *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(>>)
}

static Value int_iadd(Value *this, Value *other)
{
	INT_IBINOP_FUNC_BODY(+)
}

static Value int_isub(Value *this, Value *other)
{
	INT_IBINOP_FUNC_BODY(-)
}

static Value int_imul(Value *this, Value *other)
{
	INT_IBINOP_FUNC_BODY(*)
}

static Value int_idiv(Value *this, Value *other)
{
	if (isint(other) && !intvalue(other)) {
		return makedbz();
	}
	INT_IBINOP_FUNC_BODY(/)
}

static Value int_imod(Value *this, Value *other)
{
	if (isint(other) && !intvalue(other)) {
		return makedbz();
	}
	INT_IBINOP_FUNC_BODY_NOFLOAT(%)
}

static Value int_ipow(Value *this, Value *other)
{
	if (isint(other)) {
		intvalue(this) = pow(intvalue(this), intvalue(other));
		return *this;
	} else if (isfloat(other)) {
		this->type = VAL_TYPE_FLOAT;
		floatvalue(this) = pow(intvalue(this), floatvalue(other));
		return *this;
	} else {
		return makeut();
	}
}

static Value int_ibitand(Value *this, Value *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(&)
}

static Value int_ibitor(Value *this, Value *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(|)
}

static Value int_ixor(Value *this, Value *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(^)
}

static Value int_ishiftl(Value *this, Value *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(<<)
}

static Value int_ishiftr(Value *this, Value *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(>>)
}

static bool int_nonzero(Value *this)
{
	return intvalue(this) != 0;
}

static Value int_to_int(Value *this)
{
	return *this;
}

static Value int_to_float(Value *this)
{
	return makefloat(intvalue(this));
}

static void int_str(Value *this, Str *dest)
{
	char buf[32];
	const long n = intvalue(this);
	const size_t len = sprintf(buf, "%ld", n);
	assert(len > 0);
	char *copy = rho_malloc(len + 1);
	strcpy(copy, buf);
	*dest = STR_INIT(copy, len, 1);
}

struct num_methods int_num_methods = {
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

Class int_class = {
	.base = CLASS_BASE_INIT(),
	.name = "Int",
	.super = &obj_class,

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

	.num_methods = &int_num_methods,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
