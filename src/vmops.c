#include <stdlib.h>
#include <stdbool.h>
#include "object.h"
#include "err.h"
#include "vmops.h"

/*
 * General operations
 * ------------------
 */

#define MAKE_VM_BINOP(op, tok) \
Value op_##op(Value *a, Value *b) \
{ \
	Class *class = getclass(a); \
	BinOp binop = resolve_##op(class); \
	bool r_op = false; \
	Value result; \
\
	if (!binop) { \
		Class *class2 = getclass(b); \
		binop = resolve_r##op(class2); \
\
		if (!binop) { \
			goto error; \
		} \
\
		r_op = true; \
		result = binop(b, a); \
	} else { \
		result = binop(a, b); \
	} \
\
	if (isut(&result)) { \
		if (r_op) { \
			goto error; \
		} \
\
		Class *class2 = getclass(b); \
		binop = resolve_r##op(class2); \
\
		if (!binop) { \
			goto error; \
		} \
\
		result = binop(b, a); \
\
		if (isut(&result)) { \
			goto error; \
		} \
	} \
\
	return result; \
\
	error: \
	return makeerr(type_error_unsupported_2(#tok, class, getclass(b))); \
}

#define MAKE_VM_BINOP_DBZ_AWARE(op, tok) \
Value op_##op(Value *a, Value *b) \
{ \
	Class *class = getclass(a); \
	BinOp binop = resolve_##op(class); \
	bool r_op = false; \
	Value result; \
\
	if (!binop) { \
		Class *class2 = getclass(b); \
		binop = resolve_r##op(class2); \
\
		if (!binop) { \
			goto type_error; \
		} \
\
		r_op = true; \
		result = binop(b, a); \
	} else { \
		result = binop(a, b); \
	} \
\
	if (isut(&result)) { \
		if (r_op) { \
			goto type_error; \
		} \
\
		Class *class2 = getclass(b); \
		binop = resolve_r##op(class2); \
\
		if (!binop) { \
			goto type_error; \
		} \
\
		result = binop(b, a); \
\
		if (isut(&result)) { \
			goto type_error; \
		} \
	} \
\
	if (isdbz(&result)) { \
		goto div_by_zero_error; \
	} \
\
	return result; \
\
	type_error: \
	return makeerr(type_error_unsupported_2(#tok, class, getclass(b))); \
\
	div_by_zero_error: \
	return makeerr(div_by_zero_error()); \
}

#define MAKE_VM_UNOP(op, tok) \
Value op_##op(Value *a) \
{ \
	Class *class = getclass(a); \
	const UnOp unop = resolve_##op(class); \
\
	if (!unop) { \
		return makeerr(type_error_unsupported_1(#tok, class)); \
	} \
\
	return unop(a); \
}

MAKE_VM_BINOP(add, +)
MAKE_VM_BINOP(sub, -)
MAKE_VM_BINOP(mul, *)
MAKE_VM_BINOP_DBZ_AWARE(div, /)
MAKE_VM_BINOP_DBZ_AWARE(mod, %)
MAKE_VM_BINOP(pow, **)
MAKE_VM_BINOP(bitand, &)
MAKE_VM_BINOP(bitor, |)
MAKE_VM_BINOP(xor, ^)
MAKE_VM_UNOP(bitnot, ~)
MAKE_VM_BINOP(shiftl, <<)
MAKE_VM_BINOP(shiftr, >>)

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

#define MAKE_VM_CMPOP(op, tok) \
Value op_##op(Value *a, Value *b) \
{ \
	Class *class = getclass(a); \
	const BinOp cmp = resolve_cmp(class); \
\
	if (!cmp) { \
		goto error; \
	} \
\
	Value v = cmp(a, b); \
\
	if (isut(&v)) { \
		goto error; \
	} \
\
	if (!isint(&v)) { \
		return makeerr(type_error_invalid_cmp(class)); \
	} \
\
	return makeint(intvalue(&v) tok 0); \
\
	error: \
	return makeerr(type_error_unsupported_2(#tok, class, getclass(b))); \
}

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

MAKE_VM_CMPOP(lt, <)
MAKE_VM_CMPOP(gt, >)
MAKE_VM_CMPOP(le, <=)
MAKE_VM_CMPOP(ge, >=)

/*
 * Other unary operations
 * ----------------------
 */

MAKE_VM_UNOP(plus, unary +)
MAKE_VM_UNOP(minus, unary -)

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

#define MAKE_VM_IBINOP(op, tok) \
Value op_i##op(Value *a, Value *b) \
{ \
	Class *class = getclass(a); \
	BinOp binop = resolve_i##op(class); \
	bool r_op = false; \
	bool i_op = true; \
	Value result; \
\
	if (!binop) { \
		binop = resolve_##op(class); \
		if (!binop) { \
			Class *class2 = getclass(b); \
			binop = resolve_r##op(class2); \
\
			if (!binop) { \
				goto error; \
			} else { \
				result = binop(b, a); \
			} \
\
			r_op = true; \
		} else { \
			result = binop(a, b); \
		} \
		i_op = false; \
	} else { \
		result = binop(a, b); \
	} \
\
	while (isut(&result)) { \
		if (i_op) { \
			binop = resolve_##op(class); \
\
			if (!binop) { \
				Class *class2 = getclass(b); \
				binop = resolve_r##op(class2); \
\
				if (!binop) { \
					goto error; \
				} else { \
					result = binop(b, a); \
				} \
\
				r_op = true; \
			} else { \
				result = binop(a, b); \
			} \
\
			i_op = false; \
		} else if (r_op) { \
			goto error; \
		} else { \
			Class *class2 = getclass(b); \
			binop = resolve_r##op(class2); \
\
			if (!binop) { \
				goto error; \
			} else { \
				result = binop(b, a); \
			} \
\
			r_op = true; \
		} \
	} \
\
	if (!i_op) { \
		release(a); \
	} \
\
	return result; \
\
	error: \
	return makeerr(type_error_unsupported_2(#tok, class, getclass(b))); \
}

#define MAKE_VM_IBINOP_DBZ_AWARE(op, tok) \
Value op_i##op(Value *a, Value *b) \
{ \
	Class *class = getclass(a); \
	BinOp binop = resolve_i##op(class); \
	bool r_op = false; \
	bool i_op = true; \
	Value result; \
\
	if (!binop) { \
		binop = resolve_##op(class); \
		if (!binop) { \
			Class *class2 = getclass(b); \
			binop = resolve_r##op(class2); \
\
			if (!binop) { \
				goto type_error; \
			} else { \
				result = binop(b, a); \
			} \
\
			r_op = true; \
		} else { \
			result = binop(a, b); \
		} \
		i_op = false; \
	} else { \
		result = binop(a, b); \
	} \
\
	while (isut(&result)) { \
		if (i_op) { \
			binop = resolve_##op(class); \
\
			if (!binop) { \
				Class *class2 = getclass(b); \
				binop = resolve_r##op(class2); \
\
				if (!binop) { \
					goto type_error; \
				} else { \
					result = binop(b, a); \
				} \
\
				r_op = true; \
			} else { \
				result = binop(a, b); \
			} \
\
			i_op = false; \
		} else if (r_op) { \
			goto type_error; \
		} else { \
			Class *class2 = getclass(b); \
			binop = resolve_r##op(class2); \
\
			if (!binop) { \
				goto type_error; \
			} else { \
				result = binop(b, a); \
			} \
\
			r_op = true; \
		} \
	} \
\
	if (isdbz(&result)) { \
		goto div_by_zero_error; \
	} \
\
	if (!i_op) { \
		release(a); \
	} \
\
	return result; \
\
	type_error: \
	return makeerr(type_error_unsupported_2(#tok, class, getclass(b))); \
\
	div_by_zero_error: \
	return makeerr(div_by_zero_error()); \
}

MAKE_VM_IBINOP(add, +=)
MAKE_VM_IBINOP(sub, -=)
MAKE_VM_IBINOP(mul, *=)
MAKE_VM_IBINOP_DBZ_AWARE(div, /=)
MAKE_VM_IBINOP_DBZ_AWARE(mod, %=)
MAKE_VM_IBINOP(pow, **=)
MAKE_VM_IBINOP(bitand, &=)
MAKE_VM_IBINOP(bitor, |=)
MAKE_VM_IBINOP(xor, ^=)
MAKE_VM_IBINOP(shiftl, <<=)
MAKE_VM_IBINOP(shiftr, >>=)
