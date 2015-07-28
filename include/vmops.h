#ifndef VMOPS_H
#define VMOPS_H

#include "str.h"
#include "object.h"

Value op_hash(Value *v);

void op_str(Value *v, Str *dest);

void op_print(Value *v, FILE *out);

Value op_add(Value *a, Value *b);

Value op_sub(Value *a, Value *b);

Value op_mul(Value *a, Value *b);

Value op_div(Value *a, Value *b);

Value op_mod(Value *a, Value *b);

Value op_pow(Value *a, Value *b);

Value op_bitand(Value *a, Value *b);

Value op_bitor(Value *a, Value *b);

Value op_xor(Value *a, Value *b);

Value op_bitnot(Value *a);

Value op_shiftl(Value *a, Value *b);

Value op_shiftr(Value *a, Value *b);

Value op_and(Value *a, Value *b);

Value op_or(Value *a, Value *b);

Value op_not(Value *a);

Value op_eq(Value *a, Value *b);

Value op_neq(Value *a, Value *b);

Value op_lt(Value *a, Value *b);

Value op_gt(Value *a, Value *b);

Value op_le(Value *a, Value *b);

Value op_ge(Value *a, Value *b);

Value op_plus(Value *a);

Value op_minus(Value *a);

Value op_iadd(Value *a, Value *b);

Value op_isub(Value *a, Value *b);

Value op_imul(Value *a, Value *b);

Value op_idiv(Value *a, Value *b);

Value op_imod(Value *a, Value *b);

Value op_ipow(Value *a, Value *b);

Value op_ibitand(Value *a, Value *b);

Value op_ibitor(Value *a, Value *b);

Value op_ixor(Value *a, Value *b);

Value op_ishiftl(Value *a, Value *b);

Value op_ishiftr(Value *a, Value *b);

Value op_get(Value *v, Value *idx);

Value op_set(Value *v, Value *idx, Value *e);

Value op_get_attr(Value *v, const char *attr);

Value op_get_attr_default(Value *v, const char *attr);

Value op_set_attr(Value *v, const char *attr, Value *new);

Value op_set_attr_default(Value *v, const char *attr, Value *new);

Value op_call(Value *v, Value *args, const size_t nargs);

#endif /* VMOPS_H */
