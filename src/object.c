#include <stdlib.h>
#include <stdio.h>
#include "err.h"
#include "util.h"
#include "intobject.h"
#include "floatobject.h"
#include "object.h"

static Value obj_hash(Value *this)
{
	return (Value){.type = VAL_TYPE_INT, .data = {.i = hash_ptr(this->data.o)}};
}

static Value obj_eq(Value *this, Value *other)
{
	type_assert(other, &obj_class);
	return (Value){.type = VAL_TYPE_INT, .data = {.i = (this->data.o == other->data.o)}};
}

static void obj_free(Value *this)
{
	free(this->data.o);
}

struct num_methods obj_num_methods = {
	NULL,    /* plus */
	NULL,    /* minus */
	NULL,    /* abs */

	NULL,    /* add */
	NULL,    /* sub */
	NULL,    /* mul */
	NULL,    /* div */
	NULL,    /* mod */
	NULL,    /* pow */

	NULL,    /* not */
	NULL,    /* and */
	NULL,    /* or */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	NULL,    /* iadd */
	NULL,    /* isub */
	NULL,    /* imul */
	NULL,    /* idiv */
	NULL,    /* imod */
	NULL,    /* ipow */

	NULL,    /* iand */
	NULL,    /* ior */
	NULL,    /* ixor */
	NULL,    /* ishiftl */
	NULL,    /* ishiftr */

	NULL,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct seq_methods obj_seq_methods = {
	NULL,    /* len */
	NULL,    /* conctat */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* iter */
	NULL,    /* iternext */
};

Class obj_class = {
	.name = "Object",

	.super = NULL,

	.new = NULL,
	.del = obj_free,

	.eq = obj_eq,
	.hash = obj_hash,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.num_methods = &obj_num_methods,
	.seq_methods = &obj_seq_methods
};

Class *getclass(Value *val)
{
	if (val == NULL) {
		return NULL;
	}

	switch (val->type) {
	case VAL_TYPE_EMPTY:
		return NULL;
	case VAL_TYPE_INT:
		return &int_class;
	case VAL_TYPE_FLOAT:
		return &float_class;
	case VAL_TYPE_OBJECT: {
		const Object *o = val->data.o;
		return o->class;
	}
	}
}

/*
 * Generic instanceof -- checks if the given object is
 * an instance of the given class or any of its super-
 * classes. This is subject to change if support for
 * multiple-inheritance is ever added.
 */
bool instanceof(Object *o, Class *class)
{
	return class != NULL && ((o->class == class) || instanceof(o, class->super));
}

#define MAKE_METHOD_RESOLVER_DIRECT(name, type) \
type resolve_##name(const Class *class) { \
	if (class == NULL) { \
		return NULL; \
	} \
\
	type op; \
	if ((op = class->name) == NULL) { \
		return resolve_##name(class->super); \
	} \
\
	return op; \
}

#define MAKE_METHOD_RESOLVER(name, category, type) \
type resolve_##name(const Class *class) { \
	if (class == NULL) { \
		return NULL; \
	} \
\
	type op; \
	if (class->category == NULL || \
	    (op = class->category->name) == NULL) { \
		return resolve_##name(class->super); \
	} \
\
	return op; \
}

MAKE_METHOD_RESOLVER_DIRECT(eq, BinOp)
MAKE_METHOD_RESOLVER_DIRECT(hash, UnOp)
MAKE_METHOD_RESOLVER_DIRECT(cmp, BinOp)
MAKE_METHOD_RESOLVER_DIRECT(str, UnOp)
MAKE_METHOD_RESOLVER_DIRECT(call, BinOp)

MAKE_METHOD_RESOLVER(plus, num_methods, UnOp)
MAKE_METHOD_RESOLVER(minus, num_methods, UnOp)
MAKE_METHOD_RESOLVER(abs, num_methods, UnOp)
MAKE_METHOD_RESOLVER(add, num_methods, BinOp)
MAKE_METHOD_RESOLVER(sub, num_methods, BinOp)
MAKE_METHOD_RESOLVER(mul, num_methods, BinOp)
MAKE_METHOD_RESOLVER(div, num_methods, BinOp)
MAKE_METHOD_RESOLVER(mod, num_methods, BinOp)
MAKE_METHOD_RESOLVER(pow, num_methods, BinOp)
MAKE_METHOD_RESOLVER(not, num_methods, UnOp)
MAKE_METHOD_RESOLVER(and, num_methods, BinOp)
MAKE_METHOD_RESOLVER(or, num_methods, BinOp)
MAKE_METHOD_RESOLVER(xor, num_methods, BinOp)
MAKE_METHOD_RESOLVER(shiftl, num_methods, BinOp)
MAKE_METHOD_RESOLVER(shiftr, num_methods, BinOp)
MAKE_METHOD_RESOLVER(iadd, num_methods, BinOp)
MAKE_METHOD_RESOLVER(isub, num_methods, BinOp)
MAKE_METHOD_RESOLVER(imul, num_methods, BinOp)
MAKE_METHOD_RESOLVER(idiv, num_methods, BinOp)
MAKE_METHOD_RESOLVER(imod, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ipow, num_methods, BinOp)
MAKE_METHOD_RESOLVER(iand, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ior, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ixor, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ishiftl, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ishiftr, num_methods, BinOp)
MAKE_METHOD_RESOLVER(nonzero, num_methods, UnOp)
MAKE_METHOD_RESOLVER(to_int, num_methods, UnOp)
MAKE_METHOD_RESOLVER(to_float, num_methods, UnOp)

MAKE_METHOD_RESOLVER(len, seq_methods, UnOp)
MAKE_METHOD_RESOLVER(concat, seq_methods, BinOp)
MAKE_METHOD_RESOLVER(get, seq_methods, BinOp)
MAKE_METHOD_RESOLVER(set, seq_methods, BinOp)
MAKE_METHOD_RESOLVER(contains, seq_methods, BinOp)
MAKE_METHOD_RESOLVER(iter, seq_methods, UnOp)
MAKE_METHOD_RESOLVER(iternext, seq_methods, UnOp)

#undef MAKE_METHOD_RESOLVER_DIRECT
#undef MAKE_METHOD_RESOLVER

void decref(Value *v)
{
	if (v == NULL || v->type != VAL_TYPE_OBJECT) {
		return;
	}

	Object *o = v->data.o;

	if (--o->refcnt == 0) {
		destroy(v);
	}
}

void incref(Value *v)
{
	if (v == NULL || v->type != VAL_TYPE_OBJECT) {
		return;
	}

	Object *o = v->data.o;
	++o->refcnt;
}

void destroy(Value *v)
{
	if (v == NULL || v->type != VAL_TYPE_OBJECT) {
		return;
	}

	Object *o = v->data.o;
	o->class->del(v);
	v->data.o = NULL;
}
