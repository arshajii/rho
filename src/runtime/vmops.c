#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "object.h"
#include "strobject.h"
#include "method.h"
#include "attr.h"
#include "iter.h"
#include "exc.h"
#include "err.h"
#include "util.h"
#include "vmops.h"

/*
 * General operations
 * ------------------
 */

RhoValue rho_op_hash(RhoValue *v)
{
	RhoClass *class = rho_getclass(v);
	RhoUnOp hash = rho_resolve_hash(class);

	if (!hash) {
		return rho_type_exc_unsupported_1("hash", class);
	}

	RhoValue res = hash(v);

	if (rho_iserror(&res)) {
		return res;
	}

	if (!rho_isint(&res)) {
		rho_release(&res);
		return RHO_TYPE_EXC("hash method did not return an integer value");
	}

	return res;
}

RhoValue rho_op_str(RhoValue *v)
{
	RhoClass *class = rho_getclass(v);
	RhoUnOp str = rho_resolve_str(class);
	RhoValue res = str(v);

	if (rho_iserror(&res)) {
		return res;
	}

	if (rho_getclass(&res) != &rho_str_class) {
		rho_release(&res);
		return RHO_TYPE_EXC("str method did not return a string object");
	}

	return res;
}

RhoValue rho_op_print(RhoValue *v, FILE *out)
{
	switch (v->type) {
	case RHO_VAL_TYPE_NULL:
		fprintf(out, "null\n");
		break;
	case RHO_VAL_TYPE_BOOL:
		fprintf(out, rho_boolvalue(v) ? "true\n" : "false\n");
		break;
	case RHO_VAL_TYPE_INT:
		fprintf(out, "%ld\n", rho_intvalue(v));
		break;
	case RHO_VAL_TYPE_FLOAT:
		fprintf(out, "%f\n", rho_floatvalue(v));
		break;
	case RHO_VAL_TYPE_OBJECT: {
		const RhoObject *o = rho_objvalue(v);
		RhoPrintFunc print = rho_resolve_print(o->class);

		if (print) {
			print(v, out);
		} else {
			RhoValue str_v = rho_op_str(v);

			if (rho_iserror(&str_v)) {
				return str_v;
			}

			RhoStrObject *str = rho_objvalue(&str_v);
			fprintf(out, "%s\n", str->str.value);
			rho_releaseo(str);
		}
		break;
	}
	case RHO_VAL_TYPE_EXC: {
		const RhoException *exc = rho_objvalue(v);
		fprintf(out, "%s\n", exc->msg);
		break;
	}
	case RHO_VAL_TYPE_EMPTY:
	case RHO_VAL_TYPE_ERROR:
	case RHO_VAL_TYPE_UNSUPPORTED_TYPES:
	case RHO_VAL_TYPE_DIV_BY_ZERO:
		RHO_INTERNAL_ERROR();
		break;
	}

	return rho_makeempty();
}

#define MAKE_VM_BINOP(op, tok) \
RhoValue rho_op_##op(RhoValue *a, RhoValue *b) \
{ \
	RhoClass *class = rho_getclass(a); \
	RhoBinOp binop = rho_resolve_##op(class); \
	bool r_op = false; \
	RhoValue result; \
\
	if (!binop) { \
		RhoClass *class2 = rho_getclass(b); \
		binop = rho_resolve_r##op(class2); \
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
	if (rho_iserror(&result)) { \
		return result; \
	} \
\
	if (rho_isut(&result)) { \
		if (r_op) { \
			goto type_error; \
		} \
\
		RhoClass *class2 = rho_getclass(b); \
		binop = rho_resolve_r##op(class2); \
\
		if (!binop) { \
			goto type_error; \
		} \
\
		result = binop(b, a); \
\
		if (rho_iserror(&result)) { \
			return result; \
		} \
\
		if (rho_isut(&result)) { \
			goto type_error; \
		} \
	} \
\
	if (rho_isdbz(&result)) { \
		goto div_by_zero_error; \
	} \
\
	return result; \
\
	type_error: \
	return rho_type_exc_unsupported_2(#tok, class, rho_getclass(b)); \
\
	div_by_zero_error: \
	return rho_makeerr(rho_err_div_by_zero()); \
}

#define MAKE_VM_UNOP(op, tok) \
RhoValue rho_op_##op(RhoValue *a) \
{ \
	RhoClass *class = rho_getclass(a); \
	const RhoUnOp unop = rho_resolve_##op(class); \
\
	if (!unop) { \
		return rho_type_exc_unsupported_1(#tok, class); \
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

RhoValue rho_op_and(RhoValue *a, RhoValue *b)
{
	RhoClass *class_a = rho_getclass(a);
	RhoClass *class_b = rho_getclass(b);
	const BoolUnOp bool_a = rho_resolve_nonzero(class_a);
	const BoolUnOp bool_b = rho_resolve_nonzero(class_b);

	return rho_makebool(bool_a(a) && bool_b(b));
}

RhoValue rho_op_or(RhoValue *a, RhoValue *b)
{
	RhoClass *class_a = rho_getclass(a);
	RhoClass *class_b = rho_getclass(b);
	const BoolUnOp bool_a = rho_resolve_nonzero(class_a);
	const BoolUnOp bool_b = rho_resolve_nonzero(class_b);

	return rho_makebool(bool_a(a) || bool_b(b));
}

RhoValue rho_op_not(RhoValue *a)
{
	RhoClass *class = rho_getclass(a);
	const BoolUnOp bool_a = rho_resolve_nonzero(class);
	return rho_makebool(!bool_a(a));
}

/*
 * Comparison operations
 * ---------------------
 */

#define MAKE_VM_CMPOP(op, tok) \
RhoValue rho_op_##op(RhoValue *a, RhoValue *b) \
{ \
	RhoClass *class = rho_getclass(a); \
	const RhoBinOp cmp = rho_resolve_cmp(class); \
\
	if (!cmp) { \
		goto error; \
	} \
\
	RhoValue result = cmp(a, b); \
\
	if (rho_iserror(&result)) { \
		return result; \
	} \
\
	if (rho_isut(&result)) { \
		goto error; \
	} \
\
	if (!rho_isint(&result)) { \
		return RHO_TYPE_EXC("comparison did not return an integer value"); \
	} \
\
	return rho_makeint(rho_intvalue(&result) tok 0); \
\
	error: \
	return rho_type_exc_unsupported_2(#tok, class, rho_getclass(b)); \
}

RhoValue rho_op_eq(RhoValue *a, RhoValue *b)
{
	RhoClass *class = rho_getclass(a);
	RhoBinOp eq = rho_resolve_eq(class);

	if (!eq) {
		return rho_type_exc_unsupported_2("==", class, rho_getclass(b));
	}

	RhoValue res = eq(a, b);

	if (rho_iserror(&res)) {
		return res;
	}

	if (!rho_isbool(&res)) {
		rho_release(&res);
		return RHO_TYPE_EXC("equals method did not return a boolean value");
	}

	return res;
}

RhoValue rho_op_neq(RhoValue *a, RhoValue *b)
{
	RhoClass *class = rho_getclass(a);
	RhoBinOp eq = rho_resolve_eq(class);

	if (!eq) {
		return rho_type_exc_unsupported_2("!=", class, rho_getclass(b));
	}

	RhoValue res = eq(a, b);

	if (rho_iserror(&res)) {
		return res;
	}

	if (!rho_isbool(&res)) {
		rho_release(&res);
		return RHO_TYPE_EXC("equals method did not return a boolean value");
	}

	return rho_makebool(!rho_boolvalue(&res));
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
 */

#define MAKE_VM_IBINOP(op, tok) \
RhoValue rho_op_i##op(RhoValue *a, RhoValue *b) \
{ \
	RhoClass *class = rho_getclass(a); \
	RhoBinOp binop = rho_resolve_i##op(class); \
	bool r_op = false; \
	bool i_op = true; \
	RhoValue result; \
\
	if (!binop) { \
		binop = rho_resolve_##op(class); \
		if (!binop) { \
			RhoClass *class2 = rho_getclass(b); \
			binop = rho_resolve_r##op(class2); \
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
	if (rho_iserror(&result)) { \
		return result; \
	} \
\
	while (rho_isut(&result)) { \
		if (i_op) { \
			binop = rho_resolve_##op(class); \
\
			if (!binop) { \
				RhoClass *class2 = rho_getclass(b); \
				binop = rho_resolve_r##op(class2); \
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
			RhoClass *class2 = rho_getclass(b); \
			binop = rho_resolve_r##op(class2); \
\
			if (!binop) { \
				goto type_error; \
			} else { \
				result = binop(b, a); \
			} \
\
			r_op = true; \
		} \
\
		if (rho_iserror(&result)) { \
			return result; \
		} \
	} \
\
	if (rho_isdbz(&result)) { \
		goto div_by_zero_error; \
	} \
\
	return result; \
\
	type_error: \
	return rho_type_exc_unsupported_2(#tok, class, rho_getclass(b)); \
\
	div_by_zero_error: \
	return rho_makeerr(rho_err_div_by_zero()); \
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

RhoValue rho_op_get(RhoValue *v, RhoValue *idx)
{
	RhoClass *class = rho_getclass(v);
	RhoBinOp get = rho_resolve_get(class);

	if (!get) {
		return rho_type_exc_cannot_index(class);
	}

	return get(v, idx);
}

RhoValue rho_op_set(RhoValue *v, RhoValue *idx, RhoValue *e)
{
	RhoClass *class = rho_getclass(v);
	RhoSeqSetFunc set = rho_resolve_set(class);

	if (!set) {
		return rho_type_exc_cannot_index(class);
	}

	return set(v, idx, e);
}

RhoValue rho_op_apply(RhoValue *v, RhoValue *fn)
{
	RhoClass *class = rho_getclass(v);
	RhoBinOp apply = rho_resolve_apply(class);

	if (!apply) {
		return rho_type_exc_cannot_apply(class);
	}

	RhoClass *fn_class = rho_getclass(fn);
	if (!rho_resolve_call(fn_class)) {
		return rho_type_exc_not_callable(fn_class);
	}

	return apply(v, fn);
}

RhoValue rho_op_iapply(RhoValue *v, RhoValue *fn)
{
	RhoClass *class = rho_getclass(v);
	RhoBinOp iapply = rho_resolve_iapply(class);

	RhoClass *fn_class = rho_getclass(fn);

	if (!rho_resolve_call(fn_class)) {
		return rho_type_exc_not_callable(fn_class);
	}

	if (!iapply) {
		RhoBinOp apply = rho_resolve_apply(class);

		if (!apply) {
			return rho_type_exc_cannot_apply(class);
		}

		return apply(v, fn);
	}

	return iapply(v, fn);
}

RhoValue rho_op_get_attr(RhoValue *v, const char *attr)
{
	RhoClass *class = rho_getclass(v);
	RhoAttrGetFunc attr_get = rho_resolve_attr_get(class);

	if (attr_get) {
		return attr_get(v, attr);
	} else {
		return rho_op_get_attr_default(v, attr);
	}
}

RhoValue rho_op_get_attr_default(RhoValue *v, const char *attr)
{
	RhoClass *class = rho_getclass(v);

	if (!rho_isobject(v)) {
		goto get_attr_error_not_found;
	}

	const unsigned int value = rho_attr_dict_get(&class->attr_dict, attr);

	if (!(value & RHO_ATTR_DICT_FLAG_FOUND)) {
		goto get_attr_error_not_found;
	}

	const bool is_method = (value & RHO_ATTR_DICT_FLAG_METHOD);
	const unsigned int idx = (value >> 2);

	RhoObject *o = rho_objvalue(v);
	RhoValue res;

	if (is_method) {
		const struct rho_attr_method *method = &class->methods[idx];
		res = rho_methobj_make(o, method->meth);
	} else {
		const struct rho_attr_member *member = &class->members[idx];
		const size_t offset = member->offset;

		switch (member->type) {
		case RHO_ATTR_T_CHAR: {
			char *c = rho_malloc(1);
			*c = rho_getmember(o, offset, char);
			res = rho_strobj_make(RHO_STR_INIT(c, 1, 1));
			break;
		}
		case RHO_ATTR_T_BYTE: {
			const long n = rho_getmember(o, offset, char);
			res = rho_makeint(n);
			break;
		}
		case RHO_ATTR_T_SHORT: {
			const long n = rho_getmember(o, offset, short);
			res = rho_makeint(n);
			break;
		}
		case RHO_ATTR_T_INT: {
			const long n = rho_getmember(o, offset, int);
			res = rho_makeint(n);
			break;
		}
		case RHO_ATTR_T_LONG: {
			const long n = rho_getmember(o, offset, long);
			res = rho_makeint(n);
			break;
		}
		case RHO_ATTR_T_UBYTE: {
			const long n = rho_getmember(o, offset, unsigned char);
			res = rho_makeint(n);
			break;
		}
		case RHO_ATTR_T_USHORT: {
			const long n = rho_getmember(o, offset, unsigned short);
			res = rho_makeint(n);
			break;
		}
		case RHO_ATTR_T_UINT: {
			const long n = rho_getmember(o, offset, unsigned int);
			res = rho_makeint(n);
			break;
		}
		case RHO_ATTR_T_ULONG: {
			const long n = rho_getmember(o, offset, unsigned long);
			res = rho_makeint(n);
			break;
		}
		case RHO_ATTR_T_SIZE_T: {
			const long n = rho_getmember(o, offset, size_t);
			res = rho_makeint(n);
			break;
		}
		case RHO_ATTR_T_BOOL: {
			const bool b = rho_getmember(o, offset, bool);
			res = rho_makebool(b);
			break;
		}
		case RHO_ATTR_T_FLOAT: {
			const double d = rho_getmember(o, offset, float);
			res = rho_makefloat(d);
			break;
		}
		case RHO_ATTR_T_DOUBLE: {
			const double d = rho_getmember(o, offset, double);
			res = rho_makefloat(d);
			break;
		}
		case RHO_ATTR_T_STRING: {
			char *str = rho_getmember(o, offset, char *);
			const size_t len = strlen(str);
			char *copy = rho_malloc(len);
			memcpy(copy, str, len);
			res = rho_strobj_make(RHO_STR_INIT(copy, len, 1));
			break;
		}
		case RHO_ATTR_T_OBJECT: {
			RhoObject *obj = rho_getmember(o, offset, RhoObject *);
			rho_retaino(obj);
			res = rho_makeobj(obj);
			break;
		}
		}
	}

	return res;

	get_attr_error_not_found:
	return rho_attr_exc_not_found(class, attr);
}

RhoValue rho_op_set_attr(RhoValue *v, const char *attr, RhoValue *new)
{
	RhoClass *class = rho_getclass(v);
	RhoAttrSetFunc attr_set = rho_resolve_attr_set(class);

	if (attr_set) {
		return attr_set(v, attr, new);
	} else {
		return rho_op_set_attr_default(v, attr, new);
	}
}

RhoValue rho_op_set_attr_default(RhoValue *v, const char *attr, RhoValue *new)
{
	RhoClass *v_class = rho_getclass(v);
	RhoClass *new_class = rho_getclass(new);

	if (!rho_isobject(v)) {
		goto set_attr_error_not_found;
	}

	const unsigned int value = rho_attr_dict_get(&v_class->attr_dict, attr);

	if (!(value & RHO_ATTR_DICT_FLAG_FOUND)) {
		goto set_attr_error_not_found;
	}

	const bool is_method = (value & RHO_ATTR_DICT_FLAG_METHOD);

	if (is_method) {
		goto set_attr_error_readonly;
	}

	const unsigned int idx = (value >> 2);

	const struct rho_attr_member *member = &v_class->members[idx];

	const int member_flags = member->flags;

	if (member_flags & RHO_ATTR_FLAG_READONLY) {
		goto set_attr_error_readonly;
	}

	const size_t offset = member->offset;

	RhoObject *o = rho_objvalue(v);
	char *o_raw = (char *)o;

	switch (member->type) {
	case RHO_ATTR_T_CHAR: {
		if (new_class != &rho_str_class) {
			goto set_attr_error_mismatch;
		}

		RhoStrObject *str = rho_objvalue(new);
		const size_t len = str->str.len;

		if (len != 1) {
			goto set_attr_error_mismatch;
		}

		const char c = str->str.value[0];
		char *member_raw = (char *)(o_raw + offset);
		*member_raw = c;
		break;
	}
	case RHO_ATTR_T_BYTE: {
		if (!rho_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = rho_intvalue(new);
		char *member_raw = (char *)(o_raw + offset);
		*member_raw = (char)n;
		break;
	}
	case RHO_ATTR_T_SHORT: {
		if (!rho_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = rho_intvalue(new);
		short *member_raw = (short *)(o_raw + offset);
		*member_raw = (short)n;
		break;
	}
	case RHO_ATTR_T_INT: {
		if (!rho_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = rho_intvalue(new);
		int *member_raw = (int *)(o_raw + offset);
		*member_raw = (int)n;
		break;
	}
	case RHO_ATTR_T_LONG: {
		if (!rho_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = rho_intvalue(new);
		long *member_raw = (long *)(o_raw + offset);
		*member_raw = n;
		break;
	}
	case RHO_ATTR_T_UBYTE: {
		if (!rho_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = rho_intvalue(new);
		unsigned char *member_raw = (unsigned char *)(o_raw + offset);
		*member_raw = (unsigned char)n;
		break;
	}
	case RHO_ATTR_T_USHORT: {
		if (!rho_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = rho_intvalue(new);
		unsigned short *member_raw = (unsigned short *)(o_raw + offset);
		*member_raw = (unsigned short)n;
		break;
	}
	case RHO_ATTR_T_UINT: {
		if (!rho_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = rho_intvalue(new);
		unsigned int *member_raw = (unsigned int *)(o_raw + offset);
		*member_raw = (unsigned int)n;
		break;
	}
	case RHO_ATTR_T_ULONG: {
		if (!rho_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = rho_intvalue(new);
		unsigned long *member_raw = (unsigned long *)(o_raw + offset);
		*member_raw = (unsigned long)n;
		break;
	}
	case RHO_ATTR_T_SIZE_T: {
		if (!rho_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = rho_intvalue(new);
		size_t *member_raw = (size_t *)(o_raw + offset);
		*member_raw = (size_t)n;
		break;
	}
	case RHO_ATTR_T_BOOL: {
		if (!rho_isbool(new)) {
			goto set_attr_error_mismatch;
		}
		const bool b = rho_boolvalue(new);
		bool *member_raw = (bool *)(o_raw + offset);
		*member_raw = b;
		break;
	}
	case RHO_ATTR_T_FLOAT: {
		float d;

		/* ints get promoted */
		if (rho_isint(new)) {
			d = (float)rho_intvalue(new);
		} else if (!rho_isfloat(new)) {
			d = 0;
			goto set_attr_error_mismatch;
		} else {
			d = (float)rho_floatvalue(new);
		}

		float *member_raw = (float *)(o_raw + offset);
		*member_raw = d;
		break;
	}
	case RHO_ATTR_T_DOUBLE: {
		double d;

		/* ints get promoted */
		if (rho_isint(new)) {
			d = (double)rho_intvalue(new);
		} else if (!rho_isfloat(new)) {
			d = 0;
			goto set_attr_error_mismatch;
		} else {
			d = rho_floatvalue(new);
		}

		float *member_raw = (float *)(o_raw + offset);
		*member_raw = d;
		break;
	}
	case RHO_ATTR_T_STRING: {
		if (new_class != &rho_str_class) {
			goto set_attr_error_mismatch;
		}

		RhoStrObject *str = rho_objvalue(new);

		char **member_raw = (char **)(o_raw + offset);
		*member_raw = (char *)str->str.value;
		break;
	}
	case RHO_ATTR_T_OBJECT: {
		if (!rho_isobject(new)) {
			goto set_attr_error_mismatch;
		}

		RhoObject **member_raw = (RhoObject **)(o_raw + offset);

		if ((member_flags & RHO_ATTR_FLAG_TYPE_STRICT) &&
		    ((*member_raw)->class != new_class)) {

			goto set_attr_error_mismatch;
		}

		RhoObject *new_o = rho_objvalue(new);
		if (*member_raw != NULL) {
			rho_releaseo(*member_raw);
		}
		rho_retaino(new_o);
		*member_raw = new_o;
		break;
	}
	}

	return rho_makefalse();

	set_attr_error_not_found:
	return rho_attr_exc_not_found(v_class, attr);

	set_attr_error_readonly:
	return rho_attr_exc_readonly(v_class, attr);

	set_attr_error_mismatch:
	return rho_attr_exc_mismatch(v_class, attr, new_class);
}

RhoValue rho_op_call(RhoValue *v,
                     RhoValue *args,
                     RhoValue *args_named,
                     const size_t nargs,
                     const size_t nargs_named)
{
	RhoClass *class = rho_getclass(v);
	const RhoCallFunc call = rho_resolve_call(class);

	if (!call) {
		return rho_type_exc_not_callable(class);
	}

	return call(v, args, args_named, nargs, nargs_named);
}

RhoValue rho_op_in(RhoValue *element, RhoValue *collection)
{
	RhoClass *class = rho_getclass(collection);
	const RhoBinOp contains = rho_resolve_contains(class);

	if (contains) {
		RhoValue ret = contains(collection, element);

		if (rho_iserror(&ret)) {
			return ret;
		}

		if (!rho_isbool(&ret)) {
			return RHO_TYPE_EXC("contains method did not return a boolean value");
		}

		return ret;
	}

	const RhoUnOp iter_fn = rho_resolve_iter(class);

	if (!iter_fn) {
		return rho_type_exc_not_iterable(class);
	}

	RhoValue iter = iter_fn(collection);
	RhoClass *iter_class = rho_getclass(&iter);
	const RhoUnOp iternext = rho_resolve_iternext(iter_class);

	if (!iternext) {
		return rho_type_exc_not_iterator(iter_class);
	}

	while (true) {
		RhoValue next = iternext(&iter);

		if (rho_is_iter_stop(&next)) {
			break;
		}

		if (rho_iserror(&next)) {
			rho_release(&iter);
			return next;
		}

		RhoValue eq = rho_op_eq(&next, element);

		rho_release(&next);

		if (rho_iserror(&eq)) {
			rho_release(&iter);
			return eq;
		}

		if (rho_isint(&eq) && rho_intvalue(&eq) != 0) {
			rho_release(&iter);
			return rho_maketrue();
		}
	}

	rho_release(&iter);
	return rho_makefalse();
}

RhoValue rho_op_iter(RhoValue *v)
{
	RhoClass *class = rho_getclass(v);
	const RhoUnOp iter = rho_resolve_iter(class);

	if (!iter) {
		return rho_type_exc_not_iterable(class);
	}

	return iter(v);
}

RhoValue rho_op_iternext(RhoValue *v)
{
	RhoClass *class = rho_getclass(v);
	const RhoUnOp iternext = rho_resolve_iternext(class);

	if (!iternext) {
		return rho_type_exc_not_iterator(class);
	}

	return iternext(v);
}
