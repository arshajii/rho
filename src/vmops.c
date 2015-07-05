#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "object.h"
#include "strobject.h"
#include "method.h"
#include "attr.h"
#include "exc.h"
#include "err.h"
#include "vmops.h"

/*
 * General operations
 * ------------------
 */

Value op_hash(Value *v)
{
	Class *class = getclass(v);
	IntUnOp hash = resolve_hash(class);

	if (!hash) {
		return type_exc_unsupported_1("hash", class);
	}

	return makeint(hash(v));
}

void op_str(Value *v, Str *dest)
{
	Class *class = getclass(v);
	StrUnOp str = resolve_str(class);
	str(v, dest);
}

void op_print(Value *v, FILE *out)
{
	switch (v->type) {
	case VAL_TYPE_INT:
		fprintf(out, "%ld\n", intvalue(v));
		break;
	case VAL_TYPE_FLOAT:
		fprintf(out, "%f\n", floatvalue(v));
		break;
	case VAL_TYPE_OBJECT: {
		const Object *o = objvalue(v);
		PrintFunc print = resolve_print(o->class);

		if (print) {
			print(v, out);
		} else {
			const StrUnOp op = resolve_str(o->class);
			Str str;
			op(v, &str);

			fprintf(out, "%s\n", str.value);

			if (str.freeable) {
				str_dealloc(&str);
			}
		}
		break;
	}
	case VAL_TYPE_EXC: {
		const Exception *exc = objvalue(v);
		fprintf(out, "%s\n", exc->msg);
		break;
	}
	case VAL_TYPE_EMPTY:
	case VAL_TYPE_ERROR:
	case VAL_TYPE_UNSUPPORTED_TYPES:
	case VAL_TYPE_DIV_BY_ZERO:
		INTERNAL_ERROR();
		break;
	}
}

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
	return type_exc_unsupported_2(#tok, class, getclass(b)); \
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
		return type_exc_unsupported_1(#tok, class); \
	} \
\
	return unop(a); \
}

MAKE_VM_BINOP(add, +)
MAKE_VM_BINOP(sub, -)
MAKE_VM_BINOP(mul, *)
MAKE_VM_BINOP(div, /)
MAKE_VM_BINOP(mod, %)
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
	return type_exc_unsupported_2(#tok, class, getclass(b)); \
}

Value op_eq(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BoolBinOp eq = resolve_eq(class);

	if (!eq) {
		return type_exc_unsupported_2("==", class, getclass(b));
	}

	return makeint(eq(a, b));
}

Value op_neq(Value *a, Value *b)
{
	Class *class = getclass(a);
	const BoolBinOp eq = resolve_eq(class);

	if (!eq) {
		return type_exc_unsupported_2("!=", class, getclass(b));
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
	return type_exc_unsupported_2(#tok, class, getclass(b)); \
\
	div_by_zero_error: \
	return makeerr(div_by_zero_error()); \
}

MAKE_VM_IBINOP(add, +=)
MAKE_VM_IBINOP(sub, -=)
MAKE_VM_IBINOP(mul, *=)
MAKE_VM_IBINOP(div, /=)
MAKE_VM_IBINOP(mod, %=)
MAKE_VM_IBINOP(pow, **=)
MAKE_VM_IBINOP(bitand, &=)
MAKE_VM_IBINOP(bitor, |=)
MAKE_VM_IBINOP(xor, ^=)
MAKE_VM_IBINOP(shiftl, <<=)
MAKE_VM_IBINOP(shiftr, >>=)

Value op_get(Value *v, Value *idx)
{
	Class *class = getclass(v);
	BinOp get = resolve_get(class);

	if (!get) {
		return type_exc_cannot_index(class);
	}

	return get(v, idx);
}

Value op_set(Value *v, Value *idx, Value *e)
{
	Class *class = getclass(v);
	SeqSetFunc set = resolve_set(class);

	if (!set) {
		return type_exc_cannot_index(class);
	}

	return set(v, idx, e);
}

Value op_get_attr(Value *v, const char *attr)
{
	Class *class = getclass(v);

	if (!isobject(v)) {
		goto get_attr_error_not_found;
	}

	const unsigned int value = attr_dict_get(&class->attr_dict, attr);

	if (!(value & ATTR_DICT_FLAG_FOUND)) {
		goto get_attr_error_not_found;
	}

	const bool is_method = (value & ATTR_DICT_FLAG_METHOD);
	const unsigned int idx = (value >> 2);

	Object *o = objvalue(v);
	Value res;

	if (is_method) {
		const struct attr_method *method = &class->methods[idx];
		res = methobj_make(o, method->meth);
	} else {
		const struct attr_member *member = &class->members[idx];
		const size_t offset = member->offset;

		switch (member->type) {
		case ATTR_T_CHAR: {
			char *c = malloc(1);
			*c = getmember(o, offset, char);
			res = strobj_make(STR_INIT(c, 1, 1));
			break;
		}
		case ATTR_T_BYTE: {
			const long n = getmember(o, offset, char);
			res = makeint(n);
			break;
		}
		case ATTR_T_SHORT: {
			const long n = getmember(o, offset, short);
			res = makeint(n);
			break;
		}
		case ATTR_T_INT: {
			const long n = getmember(o, offset, int);
			res = makeint(n);
			break;
		}
		case ATTR_T_LONG: {
			const long n = getmember(o, offset, long);
			res = makeint(n);
			break;
		}
		case ATTR_T_UBYTE: {
			const long n = getmember(o, offset, unsigned char);
			res = makeint(n);
			break;
		}
		case ATTR_T_USHORT: {
			const long n = getmember(o, offset, unsigned short);
			res = makeint(n);
			break;
		}
		case ATTR_T_UINT: {
			const long n = getmember(o, offset, unsigned int);
			res = makeint(n);
			break;
		}
		case ATTR_T_ULONG: {
			const long n = getmember(o, offset, unsigned long);
			res = makeint(n);
			break;
		}
		case ATTR_T_SIZE_T: {
			const long n = getmember(o, offset, size_t);
			res = makeint(n);
			break;
		}
		case ATTR_T_BOOL: {
			const long n = getmember(o, offset, bool);
			res = makeint(n);
			break;
		}
		case ATTR_T_FLOAT: {
			const double d = getmember(o, offset, float);
			res = makefloat(d);
			break;
		}
		case ATTR_T_DOUBLE: {
			const double d = getmember(o, offset, double);
			res = makefloat(d);
			break;
		}
		case ATTR_T_STRING: {
			char *str = getmember(o, offset, char *);
			const size_t len = strlen(str);
			char *copy = malloc(len);
			memcpy(copy, str, len);
			res = strobj_make(STR_INIT(copy, len, 1));
			break;
		}
		case ATTR_T_OBJECT: {
			Object *obj = getmember(o, offset, Object *);
			retaino(obj);
			res = makeobj(obj);
			break;
		}
		}
	}

	return res;

	get_attr_error_not_found:
	return attr_exc_not_found(class, attr);
}

Value op_set_attr(Value *v, const char *attr, Value *new)
{
	Class *v_class = getclass(v);
	Class *new_class = getclass(new);

	if (!isobject(v)) {
		goto set_attr_error_not_found;
	}

	const unsigned int value = attr_dict_get(&v_class->attr_dict, attr);

	if (!(value & ATTR_DICT_FLAG_FOUND)) {
		goto set_attr_error_not_found;
	}

	const bool is_method = (value & ATTR_DICT_FLAG_METHOD);

	if (is_method) {
		goto set_attr_error_readonly;
	}

	const unsigned int idx = (value >> 2);

	const struct attr_member *member = &v_class->members[idx];

	const int member_flags = member->flags;

	if (member_flags & ATTR_FLAG_READONLY) {
		goto set_attr_error_readonly;
	}

	const size_t offset = member->offset;

	Object *o = objvalue(v);
	char *o_raw = (char *)o;

	switch (member->type) {
	case ATTR_T_CHAR: {
		if (new_class != &str_class) {
			goto set_attr_error_mismatch;
		}

		StrObject *str = objvalue(new);
		const size_t len = str->str.len;

		if (len != 1) {
			goto set_attr_error_mismatch;
		}

		const char c = str->str.value[0];
		char *member_raw = (char *)(o_raw + offset);
		*member_raw = c;
		break;
	}
	case ATTR_T_BYTE: {
		if (!isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = intvalue(new);
		char *member_raw = (char *)(o_raw + offset);
		*member_raw = (char)n;
		break;
	}
	case ATTR_T_SHORT: {
		if (!isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = intvalue(new);
		short *member_raw = (short *)(o_raw + offset);
		*member_raw = (short)n;
		break;
	}
	case ATTR_T_INT: {
		if (!isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = intvalue(new);
		int *member_raw = (int *)(o_raw + offset);
		*member_raw = (int)n;
		break;
	}
	case ATTR_T_LONG: {
		if (!isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = intvalue(new);
		long *member_raw = (long *)(o_raw + offset);
		*member_raw = n;
		break;
	}
	case ATTR_T_UBYTE: {
		if (!isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = intvalue(new);
		unsigned char *member_raw = (unsigned char *)(o_raw + offset);
		*member_raw = (unsigned char)n;
		break;
	}
	case ATTR_T_USHORT: {
		if (!isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = intvalue(new);
		unsigned short *member_raw = (unsigned short *)(o_raw + offset);
		*member_raw = (unsigned short)n;
		break;
	}
	case ATTR_T_UINT: {
		if (!isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = intvalue(new);
		unsigned int *member_raw = (unsigned int *)(o_raw + offset);
		*member_raw = (unsigned int)n;
		break;
	}
	case ATTR_T_ULONG: {
		if (!isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = intvalue(new);
		unsigned long *member_raw = (unsigned long *)(o_raw + offset);
		*member_raw = (unsigned long)n;
		break;
	}
	case ATTR_T_SIZE_T: {
		if (!isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = intvalue(new);
		size_t *member_raw = (size_t *)(o_raw + offset);
		*member_raw = (size_t)n;
		break;
	}
	case ATTR_T_BOOL: {
		if (!isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = intvalue(new);
		bool *member_raw = (bool *)(o_raw + offset);
		*member_raw = (bool)n;
		break;
	}
	case ATTR_T_FLOAT: {
		float d;

		/* ints get promoted */
		if (isint(new)) {
			d = (float)intvalue(new);
		} else if (!isfloat(new)) {
			d = 0;
			goto set_attr_error_mismatch;
		} else {
			d = (float)floatvalue(new);
		}

		float *member_raw = (float *)(o_raw + offset);
		*member_raw = d;
		break;
	}
	case ATTR_T_DOUBLE: {
		double d;

		/* ints get promoted */
		if (isint(new)) {
			d = (double)intvalue(new);
		} else if (!isfloat(new)) {
			d = 0;
			goto set_attr_error_mismatch;
		} else {
			d = floatvalue(new);
		}

		float *member_raw = (float *)(o_raw + offset);
		*member_raw = d;
		break;
	}
	case ATTR_T_STRING: {
		if (new_class != &str_class) {
			goto set_attr_error_mismatch;
		}

		StrObject *str = objvalue(new);

		char **member_raw = (char **)(o_raw + offset);
		*member_raw = (char *)str->str.value;
		break;
	}
	case ATTR_T_OBJECT: {
		if (!isobject(new)) {
			goto set_attr_error_mismatch;
		}

		Object **member_raw = (Object **)(o_raw + offset);

		if ((member_flags & ATTR_FLAG_TYPE_STRICT) &&
		    ((*member_raw)->class != new_class)) {

			goto set_attr_error_mismatch;
		}

		Object *new_o = objvalue(new);
		if (*member_raw != NULL) {
			releaseo(*member_raw);
		}
		retaino(new_o);
		*member_raw = new_o;
		break;
	}
	}

	return makeint(0);

	set_attr_error_not_found:
	return attr_exc_not_found(v_class, attr);

	set_attr_error_readonly:
	return attr_exc_readonly(v_class, attr);

	set_attr_error_mismatch:
	return attr_exc_mismatch(v_class, attr, new_class);
}
