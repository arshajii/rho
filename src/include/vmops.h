#ifndef RHO_VMOPS_H
#define RHO_VMOPS_H

#include "strobject.h"
#include "object.h"

RhoValue rho_op_hash(RhoValue *v);

RhoValue rho_op_str(RhoValue *v);

RhoValue rho_op_print(RhoValue *v, FILE *out);

RhoValue rho_op_add(RhoValue *a, RhoValue *b);

RhoValue rho_op_sub(RhoValue *a, RhoValue *b);

RhoValue rho_op_mul(RhoValue *a, RhoValue *b);

RhoValue rho_op_div(RhoValue *a, RhoValue *b);

RhoValue rho_op_mod(RhoValue *a, RhoValue *b);

RhoValue rho_op_pow(RhoValue *a, RhoValue *b);

RhoValue rho_op_bitand(RhoValue *a, RhoValue *b);

RhoValue rho_op_bitor(RhoValue *a, RhoValue *b);

RhoValue rho_op_xor(RhoValue *a, RhoValue *b);

RhoValue rho_op_bitnot(RhoValue *a);

RhoValue rho_op_shiftl(RhoValue *a, RhoValue *b);

RhoValue rho_op_shiftr(RhoValue *a, RhoValue *b);

RhoValue rho_op_and(RhoValue *a, RhoValue *b);

RhoValue rho_op_or(RhoValue *a, RhoValue *b);

RhoValue rho_op_not(RhoValue *a);

RhoValue rho_op_eq(RhoValue *a, RhoValue *b);

RhoValue rho_op_neq(RhoValue *a, RhoValue *b);

RhoValue rho_op_lt(RhoValue *a, RhoValue *b);

RhoValue rho_op_gt(RhoValue *a, RhoValue *b);

RhoValue rho_op_le(RhoValue *a, RhoValue *b);

RhoValue rho_op_ge(RhoValue *a, RhoValue *b);

RhoValue rho_op_plus(RhoValue *a);

RhoValue rho_op_minus(RhoValue *a);

RhoValue rho_op_iadd(RhoValue *a, RhoValue *b);

RhoValue rho_op_isub(RhoValue *a, RhoValue *b);

RhoValue rho_op_imul(RhoValue *a, RhoValue *b);

RhoValue rho_op_idiv(RhoValue *a, RhoValue *b);

RhoValue rho_op_imod(RhoValue *a, RhoValue *b);

RhoValue rho_op_ipow(RhoValue *a, RhoValue *b);

RhoValue rho_op_ibitand(RhoValue *a, RhoValue *b);

RhoValue rho_op_ibitor(RhoValue *a, RhoValue *b);

RhoValue rho_op_ixor(RhoValue *a, RhoValue *b);

RhoValue rho_op_ishiftl(RhoValue *a, RhoValue *b);

RhoValue rho_op_ishiftr(RhoValue *a, RhoValue *b);

RhoValue rho_op_get(RhoValue *v, RhoValue *idx);

RhoValue rho_op_set(RhoValue *v, RhoValue *idx, RhoValue *e);

RhoValue rho_op_apply(RhoValue *v, RhoValue *fn);

RhoValue rho_op_iapply(RhoValue *v, RhoValue *fn);

RhoValue rho_op_get_attr(RhoValue *v, const char *attr);

RhoValue rho_op_get_attr_default(RhoValue *v, const char *attr);

RhoValue rho_op_set_attr(RhoValue *v, const char *attr, RhoValue *new);

RhoValue rho_op_set_attr_default(RhoValue *v, const char *attr, RhoValue *new);

RhoValue rho_op_call(RhoValue *v,
                 RhoValue *args,
                 RhoValue *args_named,
                 const size_t nargs,
                 const size_t nargs_named);

RhoValue rho_op_in(RhoValue *element, RhoValue *collection);

RhoValue rho_op_iter(RhoValue *v);

RhoValue rho_op_iternext(RhoValue *v);

#endif /* RHO_VMOPS_H */
