#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "object.h"
#include "util.h"
#include "err.h"
#include "floatobject.h"

#define TYPE_ERR_STR(op) "invalid operator types for operator " #op "."

#define FLOAT_BINOP_FUNC_BODY(op) \
	if (isint(other)) { \
		return makefloat(floatvalue(this) op intvalue(other)); \
	} else if (isfloat(other)) { \
		return makefloat(floatvalue(this) op floatvalue(other)); \
	} else { \
		return makeut(); \
	}

#define FLOAT_IBINOP_FUNC_BODY(op) \
	if (isint(other)) { \
		floatvalue(this) = floatvalue(this) op intvalue(other); \
		return *this; \
	} else if (isfloat(other)) { \
		floatvalue(this) = floatvalue(this) op floatvalue(other); \
		return *this; \
	} else { \
		return makeut(); \
	}

static bool float_eq(Value *this, Value *other)
{
	if (isint(other)) {
		return floatvalue(this) == intvalue(other);
	} else if (isfloat(other)) {
		return floatvalue(this) == floatvalue(other);
	} else {
		return false;
	}
}

static int float_hash(Value *this)
{
	return hash_double(floatvalue(this));
}

static Value float_cmp(Value *this, Value *other)
{
	const double x = floatvalue(this);
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

static Value float_plus(Value *this)
{
	return *this;
}

static Value float_minus(Value *this)
{
	return makefloat(-floatvalue(this));
}

static Value float_abs(Value *this)
{
	return makefloat(abs(floatvalue(this)));
}

static Value float_add(Value *this, Value *other)
{
	FLOAT_BINOP_FUNC_BODY(+)
}

static Value float_sub(Value *this, Value *other)
{
	FLOAT_BINOP_FUNC_BODY(-)
}

static Value float_mul(Value *this, Value *other)
{
	FLOAT_BINOP_FUNC_BODY(*)
}

static Value float_div(Value *this, Value *other)
{
	FLOAT_BINOP_FUNC_BODY(/)
}

static Value float_pow(Value *this, Value *other)
{
	if (isint(other)) {
		return makefloat(pow(floatvalue(this), intvalue(other)));
	} else if (isfloat(other)) {
		return makefloat(pow(floatvalue(this), floatvalue(other)));
	} else {
		return makeut();
	}
}

static Value float_iadd(Value *this, Value *other)
{
	FLOAT_IBINOP_FUNC_BODY(+)
}

static Value float_isub(Value *this, Value *other)
{
	FLOAT_IBINOP_FUNC_BODY(-)
}

static Value float_imul(Value *this, Value *other)
{
	FLOAT_IBINOP_FUNC_BODY(*)
}

static Value float_idiv(Value *this, Value *other)
{
	FLOAT_IBINOP_FUNC_BODY(/)
}

static Value float_ipow(Value *this, Value *other)
{
	if (isint(other)) {
		floatvalue(this) = pow(floatvalue(this), intvalue(other));
		return *this;
	} else if (isfloat(other)) {
		floatvalue(this) = pow(floatvalue(this), floatvalue(other));
		return *this;
	} else {
		return makeut();
	}
}

static bool float_nonzero(Value *this)
{
	return floatvalue(this) != 0;
}

static Value float_to_int(Value *this)
{
	return makeint(floatvalue(this));
}

static Value float_to_float(Value *this)
{
	return *this;
}

struct num_methods float_num_methods = {
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

	float_nonzero,    /* nonzero */

	float_to_int,    /* to_int */
	float_to_float,    /* to_float */
};

struct seq_methods float_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* iter */
	NULL,    /* iternext */
};

Class float_class = {
	.name = "Float",

	.super = &obj_class,

	.instance_size = 0,

	.init = NULL,
	.del = NULL,

	.eq = float_eq,
	.hash = float_hash,
	.cmp = float_cmp,
	.str = NULL,
	.call = NULL,

	.num_methods = &float_num_methods,
	.seq_methods  = &float_seq_methods,

	.members = NULL,
	.methods = NULL
};
