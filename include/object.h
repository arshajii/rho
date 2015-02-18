#ifndef OBJECT_H
#define OBJECT_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "str.h"
#include "attr.h"

typedef struct value Value;

typedef Value (*UnOp)(Value *this);
typedef Value (*BinOp)(Value *this, Value *other);
typedef int (*IntUnOp)(Value *this);
typedef bool (*BoolUnOp)(Value *this);
typedef bool (*BoolBinOp)(Value *this, Value *other);
typedef void (*StrUnOp)(Value *this, Str *dest);

typedef void (*InitFunc)(Value *this, Value *args, size_t nargs);
typedef void (*DelFunc)(Value *this);
typedef Value (*CallFunc)(Value *this, Value *args, size_t nargs);
typedef int (*PrintFunc)(Value *this, FILE *out);
typedef size_t (*LenFunc)(Value *this);
typedef Value (*SeqSetFunc)(Value *this, Value *idx, Value *v);

struct num_methods {
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

struct seq_methods {
	LenFunc len;
	BinOp get;
	SeqSetFunc set;
	BoolBinOp contains;
};

typedef struct class {
	const char *name;

	struct class *super;

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

	struct num_methods *num_methods;
	struct seq_methods *seq_methods;

	struct attr_member *members;
	struct attr_method *methods;
	AttrDict attr_dict;
} Class;

struct object;
typedef struct object Object;

extern struct num_methods obj_num_methods;
extern struct seq_methods obj_seq_methods;
extern Class obj_class;

struct object {
	Class *class;
	unsigned int refcnt;
};

struct error;

struct value {
	enum {
		/* nonexistent value */
		VAL_TYPE_EMPTY = 0,

		/* standard type classes */
		VAL_TYPE_INT,
		VAL_TYPE_FLOAT,
		VAL_TYPE_OBJECT,

		/* flags */
		VAL_TYPE_ERROR,
		VAL_TYPE_UNSUPPORTED_TYPES,
		VAL_TYPE_DIV_BY_ZERO
	} type;

	union {
		long i;
		double f;
		void *o;
		struct error *e;
	} data;
};

#define isempty(val)    ((val)->type == VAL_TYPE_EMPTY)
#define isint(val)      ((val)->type == VAL_TYPE_INT)
#define isfloat(val)    ((val)->type == VAL_TYPE_FLOAT)
#define isobject(val)   ((val)->type == VAL_TYPE_OBJECT)

#define iserror(val)    ((val)->type == VAL_TYPE_ERROR)
#define isut(val)       ((val)->type == VAL_TYPE_UNSUPPORTED_TYPES)
#define isdbz(val)      ((val)->type == VAL_TYPE_DIV_BY_ZERO)

#define intvalue(val)   ((val)->data.i)
#define floatvalue(val) ((val)->data.f)
#define objvalue(val)   ((val)->data.o)
#define errvalue(val)   ((val)->data.e)

#define makeempty()     ((Value){.type = VAL_TYPE_EMPTY, .data = {.i = 0}})
#define makeint(val)    ((Value){.type = VAL_TYPE_INT, .data = {.i = (val)}})
#define makefloat(val)  ((Value){.type = VAL_TYPE_FLOAT, .data = {.f = (val)}})
#define makeobj(val)    ((Value){.type = VAL_TYPE_OBJECT, .data = {.o = (val)}})

#define makeerr(val)    ((Value){.type = VAL_TYPE_ERROR, .data = {.e = (val)}})
#define makeut()        ((Value){.type = VAL_TYPE_UNSUPPORTED_TYPES})
#define makedbz()       ((Value){.type = VAL_TYPE_DIV_BY_ZERO})

Class *getclass(Value *v);

bool is_a(Value *v, Class *class);

BoolBinOp resolve_eq(Class *class);
IntUnOp resolve_hash(Class *class);
BinOp resolve_cmp(Class *class);
StrUnOp resolve_str(Class *class);
CallFunc resolve_call(Class *class);
PrintFunc resolve_print(Class *class);

UnOp resolve_plus(Class *class);
UnOp resolve_minus(Class *class);
UnOp resolve_abs(Class *class);
BinOp resolve_add(Class *class);
BinOp resolve_sub(Class *class);
BinOp resolve_mul(Class *class);
BinOp resolve_div(Class *class);
BinOp resolve_mod(Class *class);
BinOp resolve_pow(Class *class);
UnOp resolve_bitnot(Class *class);
BinOp resolve_bitand(Class *class);
BinOp resolve_bitor(Class *class);
BinOp resolve_xor(Class *class);
BinOp resolve_shiftl(Class *class);
BinOp resolve_shiftr(Class *class);
BinOp resolve_iadd(Class *class);
BinOp resolve_isub(Class *class);
BinOp resolve_imul(Class *class);
BinOp resolve_idiv(Class *class);
BinOp resolve_imod(Class *class);
BinOp resolve_ipow(Class *class);
BinOp resolve_ibitand(Class *class);
BinOp resolve_ibitor(Class *class);
BinOp resolve_ixor(Class *class);
BinOp resolve_ishiftl(Class *class);
BinOp resolve_ishiftr(Class *class);
BinOp resolve_radd(Class *class);
BinOp resolve_rsub(Class *class);
BinOp resolve_rmul(Class *class);
BinOp resolve_rdiv(Class *class);
BinOp resolve_rmod(Class *class);
BinOp resolve_rpow(Class *class);
BinOp resolve_rbitand(Class *class);
BinOp resolve_rbitor(Class *class);
BinOp resolve_rxor(Class *class);
BinOp resolve_rshiftl(Class *class);
BinOp resolve_rshiftr(Class *class);
BoolUnOp resolve_nonzero(Class *class);
UnOp resolve_to_int(Class *class);
UnOp resolve_to_float(Class *class);

LenFunc resolve_len(Class *class);
BinOp resolve_get(Class *class);
SeqSetFunc resolve_set(Class *class);
BoolBinOp resolve_contains (Class *class);
UnOp resolve_iter(Class *class);
UnOp resolve_iternext(Class *class);

void *obj_alloc(Class *class);
Value instantiate(Class *class, Value *args, size_t nargs);

void retaino(Object *o);
void releaseo(Object *o);
void destroyo(Object *o);

void retain(Value *v);
void release(Value *v);
void destroy(Value *v);

struct value_array {
	Value *array;
	size_t length;
};

void class_init(Class *class);

#endif /* OBJECT_H */
