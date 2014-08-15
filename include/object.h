#ifndef OBJECT_H
#define OBJECT_H

#include "str.h"

typedef struct value Value;

typedef Value (*UnOp)(Value *this);
typedef Value (*BinOp)(Value *this, Value *other);

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

	UnOp not;
	BinOp and;
	BinOp or;
	BinOp xor;
	BinOp shiftl;
	BinOp shiftr;

	BinOp iadd;
	BinOp isub;
	BinOp imul;
	BinOp idiv;
	BinOp imod;
	BinOp ipow;

	BinOp iand;
	BinOp ior;
	BinOp ixor;
	BinOp ishiftl;
	BinOp ishiftr;

	UnOp nonzero;

	UnOp to_int;
	UnOp to_float;
};

struct seq_methods {
	UnOp len;
	BinOp concat;
	BinOp get;
	BinOp set;
	BinOp contains;
	UnOp iter;
	UnOp iternext;
};

typedef struct class {
	const char *name;

	struct class *super;

	Value (*new)(Value *args, size_t nargs);
	void (*del)(Value *this);

	BinOp eq;
	UnOp hash;
	BinOp cmp;
	UnOp str;
	BinOp call;

	struct num_methods *num_methods;
	struct seq_methods *seq_methods;
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

#define intvalue(val)		((val)->data.i)
#define floatvalue(val)		((val)->data.f)
#define strvalue(val)		((val)->data.s)
#define objectvalue(val)	((val)->data.o)

#define isempty(val) 	((val)->type == VAL_TYPE_EMPTY)
#define isint(val) 		((val)->type == VAL_TYPE_INT)
#define isfloat(val) 	((val)->type == VAL_TYPE_DOUBLE)
#define isobject(val) 	((val)->type == VAL_TYPE_OBJECT)

struct value {
	enum {
		VAL_TYPE_EMPTY = 0,  // indicates nonexistent value (should always be 0)
		VAL_TYPE_INT,
		VAL_TYPE_FLOAT,
		VAL_TYPE_OBJECT
	} type;

	union {
		int i;
		double f;
		void *o;
	} data;
};

bool instanceof(Object *o, Class *class);

BinOp resolve_eq(const Class *class);
UnOp resolve_hash(const Class *class);
BinOp resolve_cmp(const Class *class);
UnOp resolve_str(const Class *class);
BinOp resolve_call(const Class *class);

UnOp resolve_plus(const Class *class);
UnOp resolve_minus(const Class *class);
UnOp resolve_abs(const Class *class);
BinOp resolve_add(const Class *class);
BinOp resolve_sub(const Class *class);
BinOp resolve_mul(const Class *class);
BinOp resolve_div(const Class *class);
BinOp resolve_mod(const Class *class);
BinOp resolve_pow(const Class *class);
UnOp resolve_not(const Class *class);
BinOp resolve_and(const Class *class);
BinOp resolve_or(const Class *class);
BinOp resolve_xor(const Class *class);
BinOp resolve_shiftl(const Class *class);
BinOp resolve_shiftr(const Class *class);
BinOp resolve_iadd(const Class *class);
BinOp resolve_isub(const Class *class);
BinOp resolve_imul(const Class *class);
BinOp resolve_idiv(const Class *class);
BinOp resolve_imod(const Class *class);
BinOp resolve_ipow(const Class *class);
BinOp resolve_iand(const Class *class);
BinOp resolve_ior(const Class *class);
BinOp resolve_ixor(const Class *class);
BinOp resolve_ishiftl(const Class *class);
BinOp resolve_ishiftr(const Class *class);
UnOp resolve_nonzero(const Class *class);
UnOp resolve_to_int(const Class *class);
UnOp resolve_to_float(const Class *class);

UnOp resolve_len(const Class *class);
BinOp resolve_concat(const Class *class);
BinOp resolve_get(const Class *class);
BinOp resolve_set(const Class *class);
BinOp resolve_contains (const Class *class);
UnOp resolve_iter(const Class *class);
UnOp resolve_iternext(const Class *class);

void decref(Value *v);
void incref(Value *v);
void destroy(Value *v);

struct value_array {
	Value *array;
	size_t length;
};

#endif /* OBJECT_H */
