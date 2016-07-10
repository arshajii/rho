#ifndef RHO_OBJECT_H
#define RHO_OBJECT_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "str.h"
#include "attr.h"
#include "metaclass.h"
#include "main.h"

typedef struct rho_value  RhoValue;
typedef struct rho_class  RhoClass;
typedef struct rho_object RhoObject;

typedef RhoValue (*RhoUnOp)(RhoValue *this);
typedef RhoValue (*RhoBinOp)(RhoValue *this, RhoValue *other);
typedef bool (*BoolUnOp)(RhoValue *this);
typedef struct rho_str_object *(*RhoStrUnOp)(RhoValue *this);

typedef RhoValue (*RhoInitFunc)(RhoValue *this, RhoValue *args, size_t nargs);
typedef void (*RhoDelFunc)(RhoValue *this);
typedef RhoValue (*RhoCallFunc)(RhoValue *this,
                             RhoValue *args,
                             RhoValue *args_named,
                             size_t nargs,
                             size_t nargs_named);
typedef int (*RhoPrintFunc)(RhoValue *this, FILE *out);
typedef RhoValue (*RhoSeqSetFunc)(RhoValue *this, RhoValue *idx, RhoValue *v);

typedef RhoValue (*RhoAttrGetFunc)(RhoValue *this, const char *attr);
typedef RhoValue (*RhoAttrSetFunc)(RhoValue *this, const char *attr, RhoValue *v);

#if RHO_THREADED
#include <stdatomic.h>
#endif

struct rho_object {
	struct rho_class *class;
#if RHO_THREADED
	atomic_uint refcnt;
#else
	unsigned int refcnt;
#endif
};

struct rho_num_methods;
struct rho_seq_methods;

struct rho_class {
	RhoObject base;
	const char *name;

	struct rho_class *super;

	const size_t instance_size;

	RhoInitFunc init;
	RhoDelFunc del;  /* every class should implement this */

	RhoBinOp eq;
	RhoUnOp hash;
	RhoBinOp cmp;
	RhoUnOp str;
	RhoCallFunc call;

	RhoPrintFunc print;

	RhoUnOp iter;
	RhoUnOp iternext;

	struct rho_num_methods *num_methods;
	struct rho_seq_methods *seq_methods;

	struct rho_attr_member *members;
	struct rho_attr_method *methods;
	RhoAttrDict attr_dict;

	RhoAttrGetFunc attr_get;
	RhoAttrSetFunc attr_set;
};

struct rho_num_methods {
	RhoUnOp plus;
	RhoUnOp minus;
	RhoUnOp abs;

	RhoBinOp add;
	RhoBinOp sub;
	RhoBinOp mul;
	RhoBinOp div;
	RhoBinOp mod;
	RhoBinOp pow;

	RhoUnOp bitnot;
	RhoBinOp bitand;
	RhoBinOp bitor;
	RhoBinOp xor;
	RhoBinOp shiftl;
	RhoBinOp shiftr;

	RhoBinOp iadd;
	RhoBinOp isub;
	RhoBinOp imul;
	RhoBinOp idiv;
	RhoBinOp imod;
	RhoBinOp ipow;

	RhoBinOp ibitand;
	RhoBinOp ibitor;
	RhoBinOp ixor;
	RhoBinOp ishiftl;
	RhoBinOp ishiftr;

	RhoBinOp radd;
	RhoBinOp rsub;
	RhoBinOp rmul;
	RhoBinOp rdiv;
	RhoBinOp rmod;
	RhoBinOp rpow;

	RhoBinOp rbitand;
	RhoBinOp rbitor;
	RhoBinOp rxor;
	RhoBinOp rshiftl;
	RhoBinOp rshiftr;

	BoolUnOp nonzero;

	RhoUnOp to_int;
	RhoUnOp to_float;
};

struct rho_seq_methods {
	RhoUnOp len;
	RhoBinOp get;
	RhoSeqSetFunc set;
	RhoBinOp contains;
	RhoBinOp apply;
	RhoBinOp iapply;
};

extern struct rho_num_methods obj_num_methods;
extern struct rho_seq_methods obj_seq_methods;
extern RhoClass rho_obj_class;

#define RHO_OBJ_INIT_STATIC(class_) { .class = (class_), .refcnt = -1 }
#define RHO_CLASS_BASE_INIT()       RHO_OBJ_INIT_STATIC(&rho_meta_class)

struct rho_error;

struct rho_value {
	enum {
		/* nonexistent value */
		RHO_VAL_TYPE_EMPTY = 0,

		/* standard type classes */
		RHO_VAL_TYPE_NULL,
		RHO_VAL_TYPE_BOOL,
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
		bool b;
		long i;
		double f;
		void *o;
		struct rho_error *e;
	} data;
};

#define rho_isempty(val)    ((val)->type == RHO_VAL_TYPE_EMPTY)
#define rho_isbool(val)     ((val)->type == RHO_VAL_TYPE_BOOL)
#define rho_isnull(val)     ((val)->type == RHO_VAL_TYPE_NULL)
#define rho_isint(val)      ((val)->type == RHO_VAL_TYPE_INT)
#define rho_isfloat(val)    ((val)->type == RHO_VAL_TYPE_FLOAT)
#define rho_isnumber(val)   (rho_isint(val) || rho_isfloat(val))
#define rho_isobject(val)   ((val)->type == RHO_VAL_TYPE_OBJECT)
#define rho_isexc(val)      ((val)->type == RHO_VAL_TYPE_EXC)

#define rho_iserror(val)    ((val)->type == RHO_VAL_TYPE_ERROR || (val)->type == RHO_VAL_TYPE_EXC)
#define rho_isut(val)       ((val)->type == RHO_VAL_TYPE_UNSUPPORTED_TYPES)
#define rho_isdbz(val)      ((val)->type == RHO_VAL_TYPE_DIV_BY_ZERO)

#define rho_boolvalue(val)  ((val)->data.b)
#define rho_intvalue(val)   ((val)->data.i)
#define rho_floatvalue(val) ((val)->data.f)
#define rho_objvalue(val)   ((val)->data.o)
#define rho_errvalue(val)   ((val)->data.e)

#define rho_intvalue_force(val)   (rho_isint(val) ? rho_intvalue(val) : (long)rho_floatvalue(val))
#define rho_floatvalue_force(val) (rho_isint(val) ? (double)rho_intvalue(val) : rho_floatvalue(val))

#define rho_makeempty()     ((RhoValue){.type = RHO_VAL_TYPE_EMPTY, .data = {.i = 0}})
#define rho_makenull()      ((RhoValue){.type = RHO_VAL_TYPE_NULL, .data = {.i = 0}})
#define rho_makebool(val)   ((RhoValue){.type = RHO_VAL_TYPE_BOOL, .data = {.b = (val)}})
#define rho_maketrue()      ((RhoValue){.type = RHO_VAL_TYPE_BOOL, .data = {.b = 1}})
#define rho_makefalse()     ((RhoValue){.type = RHO_VAL_TYPE_BOOL, .data = {.b = 0}})
#define rho_makeint(val)    ((RhoValue){.type = RHO_VAL_TYPE_INT, .data = {.i = (val)}})
#define rho_makefloat(val)  ((RhoValue){.type = RHO_VAL_TYPE_FLOAT, .data = {.f = (val)}})
#define rho_makeobj(val)    ((RhoValue){.type = RHO_VAL_TYPE_OBJECT, .data = {.o = (val)}})
#define rho_makeexc(val)    ((RhoValue){.type = RHO_VAL_TYPE_EXC, .data = {.o = (val)}})

#define rho_makeerr(val)    ((RhoValue){.type = RHO_VAL_TYPE_ERROR, .data = {.e = (val)}})
#define rho_makeut()        ((RhoValue){.type = RHO_VAL_TYPE_UNSUPPORTED_TYPES})
#define rho_makedbz()       ((RhoValue){.type = RHO_VAL_TYPE_DIV_BY_ZERO})

#define RHO_MAKE_EMPTY()     { .type = RHO_VAL_TYPE_EMPTY, .data = { .i = 0 } }
#define RHO_MAKE_NULL()      { .type = RHO_VAL_TYPE_NULL, .data = { .i = 0 } }
#define RHO_MAKE_BOOL(val)   { .type = RHO_VAL_TYPE_BOOL, .data = { .b = (val) } }
#define RHO_MAKE_TRUE()      { .type = RHO_VAL_TYPE_BOOL, .data = { .b = 1 } }
#define RHO_MAKE_FALSE()     { .type = RHO_VAL_TYPE_BOOL, .data = { .b = 0 } }
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

RhoInitFunc rho_resolve_init(RhoClass *class);
RhoDelFunc rho_resolve_del(RhoClass *class);
RhoBinOp rho_resolve_eq(RhoClass *class);
RhoUnOp rho_resolve_hash(RhoClass *class);
RhoBinOp rho_resolve_cmp(RhoClass *class);
RhoUnOp rho_resolve_str(RhoClass *class);
RhoCallFunc rho_resolve_call(RhoClass *class);
RhoPrintFunc rho_resolve_print(RhoClass *class);
RhoUnOp rho_resolve_iter(RhoClass *class);
RhoUnOp rho_resolve_iternext(RhoClass *class);
RhoAttrGetFunc rho_resolve_attr_get(RhoClass *class);
RhoAttrSetFunc rho_resolve_attr_set(RhoClass *class);

RhoUnOp rho_resolve_plus(RhoClass *class);
RhoUnOp rho_resolve_minus(RhoClass *class);
RhoUnOp rho_resolve_abs(RhoClass *class);
RhoBinOp rho_resolve_add(RhoClass *class);
RhoBinOp rho_resolve_sub(RhoClass *class);
RhoBinOp rho_resolve_mul(RhoClass *class);
RhoBinOp rho_resolve_div(RhoClass *class);
RhoBinOp rho_resolve_mod(RhoClass *class);
RhoBinOp rho_resolve_pow(RhoClass *class);
RhoUnOp rho_resolve_bitnot(RhoClass *class);
RhoBinOp rho_resolve_bitand(RhoClass *class);
RhoBinOp rho_resolve_bitor(RhoClass *class);
RhoBinOp rho_resolve_xor(RhoClass *class);
RhoBinOp rho_resolve_shiftl(RhoClass *class);
RhoBinOp rho_resolve_shiftr(RhoClass *class);
RhoBinOp rho_resolve_iadd(RhoClass *class);
RhoBinOp rho_resolve_isub(RhoClass *class);
RhoBinOp rho_resolve_imul(RhoClass *class);
RhoBinOp rho_resolve_idiv(RhoClass *class);
RhoBinOp rho_resolve_imod(RhoClass *class);
RhoBinOp rho_resolve_ipow(RhoClass *class);
RhoBinOp rho_resolve_ibitand(RhoClass *class);
RhoBinOp rho_resolve_ibitor(RhoClass *class);
RhoBinOp rho_resolve_ixor(RhoClass *class);
RhoBinOp rho_resolve_ishiftl(RhoClass *class);
RhoBinOp rho_resolve_ishiftr(RhoClass *class);
RhoBinOp rho_resolve_radd(RhoClass *class);
RhoBinOp rho_resolve_rsub(RhoClass *class);
RhoBinOp rho_resolve_rmul(RhoClass *class);
RhoBinOp rho_resolve_rdiv(RhoClass *class);
RhoBinOp rho_resolve_rmod(RhoClass *class);
RhoBinOp rho_resolve_rpow(RhoClass *class);
RhoBinOp rho_resolve_rbitand(RhoClass *class);
RhoBinOp rho_resolve_rbitor(RhoClass *class);
RhoBinOp rho_resolve_rxor(RhoClass *class);
RhoBinOp rho_resolve_rshiftl(RhoClass *class);
RhoBinOp rho_resolve_rshiftr(RhoClass *class);
BoolUnOp rho_resolve_nonzero(RhoClass *class);
RhoUnOp rho_resolve_to_int(RhoClass *class);
RhoUnOp rho_resolve_to_float(RhoClass *class);

RhoUnOp rho_resolve_len(RhoClass *class);
RhoBinOp rho_resolve_get(RhoClass *class);
RhoSeqSetFunc rho_resolve_set(RhoClass *class);
RhoBinOp rho_resolve_contains(RhoClass *class);
RhoBinOp rho_resolve_apply(RhoClass *class);
RhoBinOp rho_resolve_iapply(RhoClass *class);

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


/* thread safety */
#include "main.h"
#if RHO_THREADED
#include <pthread.h>
#define RHO_SAVED_TID_FIELD_NAME _saved_id
#define RHO_SAVED_TID_FIELD pthread_t RHO_SAVED_TID_FIELD_NAME;
#define RHO_CHECK_THREAD(o) \
	if ((o)->RHO_SAVED_TID_FIELD_NAME != pthread_self()) \
		return RHO_CONC_ACCS_EXC("invalid concurrent access to non-thread-safe method or function");
#define RHO_INIT_SAVED_TID_FIELD(o) (o)->RHO_SAVED_TID_FIELD_NAME = pthread_self()
#undef RHO_SAVED_TID_FIELD_NAME
#else
#define RHO_SAVED_TID_FIELD
#define RHO_CHECK_THREAD(o)
#define RHO_INIT_SAVED_TID_FIELD(o)
#endif

#endif /* RHO_OBJECT_H */
