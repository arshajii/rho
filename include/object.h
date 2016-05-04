#ifndef RHO_OBJECT_H
#define RHO_OBJECT_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "str.h"
#include "attr.h"
#include "metaclass.h"

typedef struct rho_value  RhoValue;
typedef struct rho_class  RhoClass;
typedef struct rho_object RhoObject;

typedef RhoValue (*UnOp)(RhoValue *this);
typedef RhoValue (*BinOp)(RhoValue *this, RhoValue *other);
typedef int (*IntUnOp)(RhoValue *this);
typedef bool (*BoolUnOp)(RhoValue *this);
typedef bool (*BoolBinOp)(RhoValue *this, RhoValue *other);
typedef struct rho_str_object *(*StrUnOp)(RhoValue *this);

typedef RhoValue (*InitFunc)(RhoValue *this, RhoValue *args, size_t nargs);
typedef void (*DelFunc)(RhoValue *this);
typedef RhoValue (*CallFunc)(RhoValue *this,
                             RhoValue *args,
                             RhoValue *args_named,
                             size_t nargs,
                             size_t nargs_named);
typedef int (*PrintFunc)(RhoValue *this, FILE *out);
typedef size_t (*LenFunc)(RhoValue *this);
typedef RhoValue (*SeqSetFunc)(RhoValue *this, RhoValue *idx, RhoValue *v);

typedef RhoValue (*AttrGetFunc)(RhoValue *this, const char *attr);
typedef RhoValue (*AttrSetFunc)(RhoValue *this, const char *attr, RhoValue *v);

struct rho_object {
	struct rho_class *class;
	unsigned int refcnt;
};

struct rho_num_methods;
struct rho_seq_methods;

struct rho_class {
	RhoObject base;
	const char *name;

	struct rho_class *super;

	const size_t instance_size;

	InitFunc init;
	DelFunc del;  /* every class should implement this */

	BoolBinOp eq;
	IntUnOp hash;
	BinOp cmp;
	StrUnOp str;
	CallFunc call;

	PrintFunc print;

	UnOp iter;
	UnOp iternext;

	struct rho_num_methods *num_methods;
	struct rho_seq_methods *seq_methods;

	struct rho_attr_member *members;
	struct rho_attr_method *methods;
	RhoAttrDict attr_dict;

	AttrGetFunc attr_get;
	AttrSetFunc attr_set;
};

struct rho_num_methods {
	UnOp plus;
	UnOp minus;
	UnOp abs;

	BinOp add;
	BinOp sub;
	BinOp mul;
	BinOp div;
	BinOp mod;
	BinOp pow;

	UnOp bitnot;
	BinOp bitand;
	BinOp bitor;
	BinOp xor;
	BinOp shiftl;
	BinOp shiftr;

	BinOp iadd;
	BinOp isub;
	BinOp imul;
	BinOp idiv;
	BinOp imod;
	BinOp ipow;

	BinOp ibitand;
	BinOp ibitor;
	BinOp ixor;
	BinOp ishiftl;
	BinOp ishiftr;

	BinOp radd;
	BinOp rsub;
	BinOp rmul;
	BinOp rdiv;
	BinOp rmod;
	BinOp rpow;

	BinOp rbitand;
	BinOp rbitor;
	BinOp rxor;
	BinOp rshiftl;
	BinOp rshiftr;

	BoolUnOp nonzero;

	UnOp to_int;
	UnOp to_float;
};

struct rho_seq_methods {
	LenFunc len;
	BinOp get;
	SeqSetFunc set;
	BoolBinOp contains;
	BinOp apply;
	BinOp iapply;
};

extern struct rho_num_methods obj_num_methods;
extern struct rho_seq_methods obj_seq_methods;
extern RhoClass obj_class;

#define RHO_OBJ_INIT_STATIC(class_) { .class = (class_), .refcnt = -1 }
#define RHO_CLASS_BASE_INIT()       RHO_OBJ_INIT_STATIC(&rho_meta_class)

struct rho_error;

struct rho_value {
	enum {
		/* nonexistent value */
		RHO_VAL_TYPE_EMPTY = 0,

		/* standard type classes */
		RHO_VAL_TYPE_INT,
		RHO_VAL_TYPE_FLOAT,
		RHO_VAL_TYPE_OBJECT,
		RHO_VAL_TYPE_EXC,

		/* flags */
		RHO_VAL_TYPE_ERROR,
		RHO_VAL_TYPE_UNSUPPORTED_TYPES,
		RHO_VAL_TYPE_DIV_BY_ZERO
	} type;

	union {
		long i;
		double f;
		void *o;
		struct rho_error *e;
	} data;
};

#define rho_isempty(val)    ((val)->type == RHO_VAL_TYPE_EMPTY)
#define rho_isint(val)      ((val)->type == RHO_VAL_TYPE_INT)
#define rho_isfloat(val)    ((val)->type == RHO_VAL_TYPE_FLOAT)
#define rho_isobject(val)   ((val)->type == RHO_VAL_TYPE_OBJECT)
#define rho_isexc(val)      ((val)->type == RHO_VAL_TYPE_EXC)

#define rho_iserror(val)    ((val)->type == RHO_VAL_TYPE_ERROR || (val)->type == RHO_VAL_TYPE_EXC)
#define rho_isut(val)       ((val)->type == RHO_VAL_TYPE_UNSUPPORTED_TYPES)
#define rho_isdbz(val)      ((val)->type == RHO_VAL_TYPE_DIV_BY_ZERO)

#define rho_intvalue(val)   ((val)->data.i)
#define rho_floatvalue(val) ((val)->data.f)
#define rho_objvalue(val)   ((val)->data.o)
#define rho_errvalue(val)   ((val)->data.e)

#define rho_makeempty()     ((RhoValue){.type = RHO_VAL_TYPE_EMPTY, .data = {.i = 0}})
#define rho_makeint(val)    ((RhoValue){.type = RHO_VAL_TYPE_INT, .data = {.i = (val)}})
#define rho_makefloat(val)  ((RhoValue){.type = RHO_VAL_TYPE_FLOAT, .data = {.f = (val)}})
#define rho_makeobj(val)    ((RhoValue){.type = RHO_VAL_TYPE_OBJECT, .data = {.o = (val)}})
#define rho_makeexc(val)    ((RhoValue){.type = RHO_VAL_TYPE_EXC, .data = {.o = (val)}})

#define rho_makeerr(val)    ((RhoValue){.type = RHO_VAL_TYPE_ERROR, .data = {.e = (val)}})
#define rho_makeut()        ((RhoValue){.type = RHO_VAL_TYPE_UNSUPPORTED_TYPES})
#define rho_makedbz()       ((RhoValue){.type = RHO_VAL_TYPE_DIV_BY_ZERO})

#define RHO_MAKE_EMPTY()     { .type = RHO_VAL_TYPE_EMPTY, .data = { .i = 0 } }
#define RHO_MAKE_INT(val)    { .type = RHO_VAL_TYPE_INT, .data = { .i = (val) } }
#define RHO_MAKE_FLOAT(val)  { .type = RHO_VAL_TYPE_FLOAT, .data = { .f = (val) } }
#define RHO_MAKE_OBJ(val)    { .type = RHO_VAL_TYPE_OBJECT, .data = { .o = (val) } }
#define RHO_MAKE_EXC(val)    { .type = RHO_VAL_TYPE_EXC, .data = { .o = (val) } }

#define RHO_MAKE_ERR(val)    { .type = RHO_VAL_TYPE_ERROR, .data = { .e = (val) } }
#define RHO_MAKE_UT()        { .type = RHO_VAL_TYPE_UNSUPPORTED_TYPES }
#define RHO_MAKE_DBZ()       { .type = RHO_VAL_TYPE_DIV_BY_ZERO }

RhoClass *rho_getclass(RhoValue *v);

bool rho_is_a(RhoValue *v, RhoClass *class);
bool rho_is_subclass(RhoClass *child, RhoClass *parent);

InitFunc rho_resolve_init(RhoClass *class);
DelFunc rho_resolve_del(RhoClass *class);
BoolBinOp rho_resolve_eq(RhoClass *class);
IntUnOp rho_resolve_hash(RhoClass *class);
BinOp rho_resolve_cmp(RhoClass *class);
StrUnOp rho_resolve_str(RhoClass *class);
CallFunc rho_resolve_call(RhoClass *class);
PrintFunc rho_resolve_print(RhoClass *class);
UnOp rho_resolve_iter(RhoClass *class);
UnOp rho_resolve_iternext(RhoClass *class);
AttrGetFunc rho_resolve_attr_get(RhoClass *class);
AttrSetFunc rho_resolve_attr_set(RhoClass *class);

UnOp rho_resolve_plus(RhoClass *class);
UnOp rho_resolve_minus(RhoClass *class);
UnOp rho_resolve_abs(RhoClass *class);
BinOp rho_resolve_add(RhoClass *class);
BinOp rho_resolve_sub(RhoClass *class);
BinOp rho_resolve_mul(RhoClass *class);
BinOp rho_resolve_div(RhoClass *class);
BinOp rho_resolve_mod(RhoClass *class);
BinOp rho_resolve_pow(RhoClass *class);
UnOp rho_resolve_bitnot(RhoClass *class);
BinOp rho_resolve_bitand(RhoClass *class);
BinOp rho_resolve_bitor(RhoClass *class);
BinOp rho_resolve_xor(RhoClass *class);
BinOp rho_resolve_shiftl(RhoClass *class);
BinOp rho_resolve_shiftr(RhoClass *class);
BinOp rho_resolve_iadd(RhoClass *class);
BinOp rho_resolve_isub(RhoClass *class);
BinOp rho_resolve_imul(RhoClass *class);
BinOp rho_resolve_idiv(RhoClass *class);
BinOp rho_resolve_imod(RhoClass *class);
BinOp rho_resolve_ipow(RhoClass *class);
BinOp rho_resolve_ibitand(RhoClass *class);
BinOp rho_resolve_ibitor(RhoClass *class);
BinOp rho_resolve_ixor(RhoClass *class);
BinOp rho_resolve_ishiftl(RhoClass *class);
BinOp rho_resolve_ishiftr(RhoClass *class);
BinOp rho_resolve_radd(RhoClass *class);
BinOp rho_resolve_rsub(RhoClass *class);
BinOp rho_resolve_rmul(RhoClass *class);
BinOp rho_resolve_rdiv(RhoClass *class);
BinOp rho_resolve_rmod(RhoClass *class);
BinOp rho_resolve_rpow(RhoClass *class);
BinOp rho_resolve_rbitand(RhoClass *class);
BinOp rho_resolve_rbitor(RhoClass *class);
BinOp rho_resolve_rxor(RhoClass *class);
BinOp rho_resolve_rshiftl(RhoClass *class);
BinOp rho_resolve_rshiftr(RhoClass *class);
BoolUnOp rho_resolve_nonzero(RhoClass *class);
UnOp rho_resolve_to_int(RhoClass *class);
UnOp rho_resolve_to_float(RhoClass *class);

LenFunc rho_resolve_len(RhoClass *class);
BinOp rho_resolve_get(RhoClass *class);
SeqSetFunc rho_resolve_set(RhoClass *class);
BoolBinOp rho_resolve_contains(RhoClass *class);
BinOp rho_resolve_apply(RhoClass *class);
BinOp rho_resolve_iapply(RhoClass *class);

void *rho_obj_alloc(RhoClass *class);
void *rho_obj_alloc_var(RhoClass *class, size_t extra);

RhoValue rho_class_instantiate(RhoClass *class, RhoValue *args, size_t nargs);

void rho_retaino(void *o);
void rho_releaseo(void *o);
void rho_destroyo(void *o);

void rho_retain(RhoValue *v);
void rho_release(RhoValue *v);
void rho_destroy(RhoValue *v);

struct rho_value_array {
	RhoValue *array;
	size_t length;
};

void rho_class_init(RhoClass *class);

#endif /* RHO_OBJECT_H */
