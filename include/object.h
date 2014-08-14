#ifndef OBJECT_H
#define OBJECT_H

#include "str.h"

typedef struct value Value;

typedef Value (*UnOp)(Value *this);
typedef Value (*BinOp)(Value *this, Value *other);

struct arith_methods {
	UnOp plus;
	UnOp minus;
	BinOp add;
	BinOp sub;
	BinOp mul;
	BinOp div;
	BinOp mod;
	BinOp pow;
};

struct cmp_methods {
	BinOp eq;
	BinOp cmp;
	UnOp hash;
};

struct misc_methods {
	BinOp call;
	BinOp index;
	UnOp nonzero;
};

typedef struct class {
	const char *name;

	struct class *super;

	Value (*new)(Value *args, size_t nargs);
	void (*free)(Value *this);
	UnOp str;

	struct arith_methods	*arith_methods;
	struct cmp_methods		*cmp_methods;
	struct misc_methods		*misc_methods;
} Class;

struct object;
typedef struct object Object;

extern struct arith_methods obj_arith_methods;
extern struct cmp_methods obj_cmp_methods;
extern struct misc_methods obj_misc_methods;
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
		VAL_TYPE_EMPTY = 0,  // indicates nonexistent object (should always be 0)
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

UnOp resolve_plus(const Class *class);
UnOp resolve_minus(const Class *class);
BinOp resolve_add(const Class *class);
BinOp resolve_sub(const Class *class);
BinOp resolve_mul(const Class *class);
BinOp resolve_div(const Class *class);
BinOp resolve_mod(const Class *class);
BinOp resolve_pow(const Class *class);

BinOp resolve_eq(const Class *class);
BinOp resolve_cmp(const Class *class);
UnOp resolve_hash(const Class *class);

BinOp resolve_call(const Class *class);
BinOp resolve_index(const Class *class);
UnOp resolve_nonzero(const Class *class);

void decref(Value *v);
void incref(Value *v);
void destroy(Value *v);

struct value_array {
	Value *array;
	size_t length;
};

#endif /* OBJECT_H */
