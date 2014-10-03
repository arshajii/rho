#ifndef VMOPS_H
#define VMOPS_H

#include "object.h"

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

Value op_uplus(Value *a);

Value op_uminus(Value *a);

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

#endif /* VMOPS_H */
