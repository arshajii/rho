#include <stdlib.h>
#include "object.h"
#include "err.h"
#include "vmops.h"

/*
 * General operations
 * ------------------
 */

Value op_add(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp add = resolve_add(class);

	if (!add) {
		return makeerr(type_error_unsupported_2("+", class, getclass(b)));
	}

	return add(a, b);
}

Value op_sub(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp sub = resolve_sub(class);

	if (!sub) {
		return makeerr(type_error_unsupported_2("-", class, getclass(b)));
	}

	return sub(a, b);
}

Value op_mul(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp mul = resolve_mul(class);

	if (!mul) {
		return makeerr(type_error_unsupported_2("*", class, getclass(b)));
	}

	return mul(a, b);
}

Value op_div(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp div = resolve_div(class);

	if (!div) {
		return makeerr(type_error_unsupported_2("/", class, getclass(b)));
	}

	return div(a, b);
}

Value op_mod(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp mod = resolve_mod(class);

	if (!mod) {
		return makeerr(type_error_unsupported_2("%", class, getclass(b)));
	}

	return mod(a, b);
}

Value op_pow(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp pow = resolve_pow(class);

	if (!pow) {
		return makeerr(type_error_unsupported_2("**", class, getclass(b)));
	}

	return pow(a, b);
}

Value op_bitand(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp and = resolve_and(class);

	if (!and) {
		return makeerr(type_error_unsupported_2("&", class, getclass(b)));
	}

	return and(a, b);
}

Value op_bitor(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp or = resolve_or(class);

	if (!or) {
		return makeerr(type_error_unsupported_2("|", class, getclass(b)));
	}

	return or(a, b);
}

Value op_xor(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp xor = resolve_xor(class);

	if (!xor) {
		return makeerr(type_error_unsupported_2("^", class, getclass(b)));
	}

	return xor(a, b);
}

Value op_bitnot(Value *a)
{
	Class *class = getclass(a);
	const UnOp not = resolve_not(class);

	if (!not) {
		return makeerr(type_error_unsupported_1("^", class));
	}

	return not(a);
}

Value op_shiftl(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp shiftl = resolve_shiftl(class);

	if (!shiftl) {
		return makeerr(type_error_unsupported_2("<<", class, getclass(b)));
	}

	return shiftl(a, b);
}

Value op_shiftr(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp shiftr = resolve_shiftr(class);

	if (!shiftr) {
		return makeerr(type_error_unsupported_2(">>", class, getclass(b)));
	}

	return shiftr(a, b);
}

/*
 * Logical boolean operations
 * --------------------------
 * Note that the `nonzero` method should be available
 * for all types since it is defined by `obj_class`. Hence,
 * no error checking is done here.
 */

Value op_and(Value *a, Value *b)
{
	Class *class_a = getclass(a);
	Class *class_b = getclass(b);
	const BoolUnOp bool_a = resolve_nonzero(class_a);
	const BoolUnOp bool_b = resolve_nonzero(class_b);

	return makeint(bool_a(a) && bool_b(b));
}

Value op_or(Value *a, Value *b)
{
	Class *class_a = getclass(a);
	Class *class_b = getclass(b);
	const BoolUnOp bool_a = resolve_nonzero(class_a);
	const BoolUnOp bool_b = resolve_nonzero(class_b);

	return makeint(bool_a(a) || bool_b(b));
}

Value op_not(Value *a)
{
	Class *class = getclass(a);
	const BoolUnOp bool_a = resolve_nonzero(class);
	return makeint(!bool_a(a));
}

/*
 * Comparison operations
 * ---------------------
 */

Value op_eq(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BoolBinOp eq = resolve_eq(class);

	if (!eq) {
		return makeerr(type_error_unsupported_2("==", class, getclass(b)));
	}

	return makeint(eq(a, b));
}

Value op_neq(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BoolBinOp eq = resolve_eq(class);

	if (!eq) {
		return makeerr(type_error_unsupported_2("!=", class, getclass(b)));
	}

	return makeint(!eq(a, b));
}

Value op_lt(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp cmp = resolve_cmp(class);

	if (!cmp) {
		return makeerr(type_error_unsupported_2("<", class, getclass(b)));
	}

	Value v = cmp(a, b);

	if (iserror(&v)) {
		return v;
	}

	if (!isint(&v)) {
		return makeerr(type_error_invalid_cmp(class));
	}

	return makeint(intvalue(&v) < 0);
}

Value op_gt(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp cmp = resolve_cmp(class);

	if (!cmp) {
		return makeerr(type_error_unsupported_2(">", class, getclass(b)));
	}

	Value v = cmp(a, b);

	if (iserror(&v)) {
		return v;
	}

	if (!isint(&v)) {
		return makeerr(type_error_invalid_cmp(class));
	}

	return makeint(intvalue(&v) > 0);
}

Value op_le(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp cmp = resolve_cmp(class);

	if (!cmp) {
		return makeerr(type_error_unsupported_2("<=", class, getclass(b)));
	}

	Value v = cmp(a, b);

	if (iserror(&v)) {
		return v;
	}

	if (!isint(&v)) {
		return makeerr(type_error_invalid_cmp(class));
	}

	return makeint(intvalue(&v) <= 0);
}

Value op_ge(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BinOp cmp = resolve_cmp(class);

	if (!cmp) {
		return makeerr(type_error_unsupported_2(">=", class, getclass(b)));
	}

	Value v = cmp(a, b);

	if (iserror(&v)) {
		return v;
	}

	if (!isint(&v)) {
		return makeerr(type_error_invalid_cmp(class));
	}

	return makeint(intvalue(&v) >= 0);
}

/*
 * Other unary operations
 * ----------------------
 */

Value op_uplus(Value *a)
{
	Class *class = getclass(a);
	const UnOp plus = resolve_plus(class);

	if (!plus) {
		return makeerr(type_error_unsupported_1("unary +", class));
	}

	return plus(a);
}

Value op_uminus(Value *a)
{
	Class *class = getclass(a);
	const UnOp minus = resolve_minus(class);

	if (!minus) {
		return makeerr(type_error_unsupported_1("unary -", class));
	}

	return minus(a);
}

/*
 * In-place binary operations
 * --------------------------
 * These get called internally when a compound assignment is
 * performed.
 *
 * If the class of the LHS does not provide the corresponding
 * in-place method, the general binary method is used instead
 * (e.g. if a class does not provide `iadd`, `add` will be
 * looked up and used).
 *
 * Since these operations are supposedly in-place, a user of
 * these functions would not expect to have to `retain` the
 * result because it *should* simply be the LHS, which has
 * already been retained. However, as described above, if the
 * corresponding in-place method is not supported, the regular
 * method will be used, in which case a new value will be
 * returned. Therefore, in this case, the LHS will be released
 * by the in-place operator function. A consequence of this is
 * that the LHS should always be replaced by the return value
 * of the function, wherever it is being stored.
 */

Value op_iadd(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp add = resolve_iadd(class);

	bool release_a = false;

	if (!add) {
		add = resolve_add(class);
		if (!add) {
			return makeerr(type_error_unsupported_2("+=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = add(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}

Value op_isub(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp sub = resolve_isub(class);

	bool release_a = false;

	if (!sub) {
		sub = resolve_sub(class);
		if (!sub) {
			return makeerr(type_error_unsupported_2("-=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = sub(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}

Value op_imul(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp mul = resolve_imul(class);

	bool release_a = false;

	if (!mul) {
		mul = resolve_imul(class);
		if (!mul) {
			return makeerr(type_error_unsupported_2("*=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = mul(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}

Value op_idiv(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp div = resolve_idiv(class);

	bool release_a = false;

	if (!div) {
		div = resolve_div(class);
		if (!div) {
			return makeerr(type_error_unsupported_2("/=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = div(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}

Value op_imod(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp mod = resolve_imod(class);

	bool release_a = false;

	if (!mod) {
		mod = resolve_mod(class);
		if (!mod) {
			return makeerr(type_error_unsupported_2("%=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = mod(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}

Value op_ipow(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp pow = resolve_ipow(class);

	bool release_a = false;

	if (!pow) {
		pow = resolve_pow(class);
		if (!pow) {
			return makeerr(type_error_unsupported_2("**=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = pow(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}

Value op_ibitand(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp and = resolve_iand(class);

	bool release_a = false;

	if (!and) {
		and = resolve_and(class);
		if (!and) {
			return makeerr(type_error_unsupported_2("&=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = and(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}

Value op_ibitor(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp or = resolve_ior(class);

	bool release_a = false;

	if (!or) {
		or = resolve_or(class);
		if (!or) {
			return makeerr(type_error_unsupported_2("|=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = or(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}

Value op_ixor(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp xor = resolve_ixor(class);

	bool release_a = false;

	if (!xor) {
		xor = resolve_xor(class);
		if (!xor) {
			return makeerr(type_error_unsupported_2("^=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = xor(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}

Value op_ishiftl(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp shiftl = resolve_ishiftl(class);

	bool release_a = false;

	if (!shiftl) {
		shiftl = resolve_shiftl(class);
		if (!shiftl) {
			return makeerr(type_error_unsupported_2("<<=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = shiftl(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}

Value op_ishiftr(Value *a, Value *b)
{
	Class *class = getclass(a);
	BinOp shiftr = resolve_ishiftr(class);

	bool release_a = false;

	if (!shiftr) {
		shiftr = resolve_shiftr(class);
		if (!shiftr) {
			return makeerr(type_error_unsupported_2(">>=", class, getclass(b)));
		}
		release_a = true;
	}

	Value result = shiftr(a, b);
	if (release_a) {
		release(a);
	}

	return result;
}
