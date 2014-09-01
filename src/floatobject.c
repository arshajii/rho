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
		type_error(TYPE_ERR_STR(op)); \
		return SENTINEL; \
	}

#define FLOAT_IBINOP_FUNC_BODY(op) \
	if (isint(other)) { \
		this->data.f = this->data.f op intvalue(other); \
		return *this; \
	} else if (isfloat(other)) { \
		this->data.f = this->data.f op floatvalue(other); \
		return *this; \
	} else { \
		type_error(TYPE_ERR_STR(op)); \
		return SENTINEL; \
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
	return hash_float(floatvalue(this));
}

static int float_cmp(Value *this, Value *other)
{
	const double x = floatvalue(this);
	if (isint(other)) {
		const int y = intvalue(other);
		return ((x < y) ? -1 : ((x == y) ? 0 : 1));
	} else if (isfloat(other)) {
		const double y = floatvalue(other);
		return ((x < y) ? -1 : ((x == y) ? 0 : 1));
	} else {
		type_error(TYPE_ERR_STR(cmp));
		return 0;
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
		type_error(TYPE_ERR_STR(**)); \
		return SENTINEL;
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
		this->data.f = pow(this->data.f, intvalue(other));
		return *this;
	} else if (isfloat(other)) {
		this->data.f = pow(this->data.f, floatvalue(other));
		return *this;
	} else {
		type_error(TYPE_ERR_STR(**));
		return SENTINEL;
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

	NULL,    /* not */
	NULL,    /* and */
	NULL,    /* or */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	float_iadd,    /* iadd */
	float_isub,    /* isub */
	float_imul,    /* imul */
	float_idiv,    /* idiv */
	NULL,    /* imod */
	float_ipow,    /* ipow */

	NULL,    /* iand */
	NULL,    /* ior */
	NULL,    /* ixor */
	NULL,    /* ishiftl */
	NULL,    /* ishiftr */

	float_nonzero,    /* nonzero */

	float_to_int,    /* to_int */
	float_to_float,    /* to_float */
};

Class float_class = {
	.name = "Float",

	.super = &obj_class,

	.new = NULL,
	.del = NULL,

	.eq = float_eq,
	.hash = float_hash,
	.cmp = float_cmp,
	.str = NULL,
	.call = NULL,

	.num_methods = &float_num_methods,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL
};
