#include <stdlib.h>
#include <stdio.h>
#include "err.h"
#include "util.h"
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

struct arith_methods obj_arith_methods = {
	NULL,	/*  uplus   */
	NULL,	/*  uminus  */
	NULL,	/*  add     */
	NULL,	/*  sub     */
	NULL,	/*  mul     */
	NULL,	/*  div     */
	NULL,	/*  mod     */
	NULL	/*  pow     */
};

struct cmp_methods obj_cmp_methods = {
	obj_eq,     /*  eq    */
	NULL,       /*  cmp   */
	obj_hash,   /*  hash  */
};

struct misc_methods obj_misc_methods = {
	NULL,	/*  call     */
	NULL,	/*  index    */
	NULL,	/*  nonzero  */
};

Class obj_class = {
	.name = "Object",

	.super = NULL,

	.new = NULL,
	.free = obj_free,
	.str = NULL,

	.arith_methods = &obj_arith_methods,
	.cmp_methods = &obj_cmp_methods,
	.misc_methods = &obj_misc_methods
};

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
	return op; \
}

MAKE_METHOD_RESOLVER(plus, arith_methods, UnOp)
MAKE_METHOD_RESOLVER(minus, arith_methods, UnOp)
MAKE_METHOD_RESOLVER(add, arith_methods, BinOp)
MAKE_METHOD_RESOLVER(sub, arith_methods, BinOp)
MAKE_METHOD_RESOLVER(mul, arith_methods, BinOp)
MAKE_METHOD_RESOLVER(div, arith_methods, BinOp)
MAKE_METHOD_RESOLVER(mod, arith_methods, BinOp)
MAKE_METHOD_RESOLVER(pow, arith_methods, BinOp)

MAKE_METHOD_RESOLVER(eq, cmp_methods, BinOp)
MAKE_METHOD_RESOLVER(cmp, cmp_methods, BinOp)
MAKE_METHOD_RESOLVER(hash, cmp_methods, UnOp)

MAKE_METHOD_RESOLVER(call, misc_methods, BinOp)
MAKE_METHOD_RESOLVER(index, misc_methods, BinOp)
MAKE_METHOD_RESOLVER(nonzero, misc_methods, UnOp)

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
	o->class->free(v);
	v->data.o = NULL;
}
