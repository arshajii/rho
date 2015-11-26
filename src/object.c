#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "attr.h"
#include "exc.h"
#include "err.h"
#include "util.h"
#include "intobject.h"
#include "floatobject.h"
#include "exc.h"
#include "object.h"

static Value obj_init(Value *this, Value *args, size_t nargs)
{
	UNUSED(args);

	if (nargs > 0) {
		return TYPE_EXC("Object constructor takes no arguments (got %lu)", nargs);
	}

	return *this;
}

static bool obj_eq(Value *this, Value *other)
{
	if (!isobject(other)) {
		return false;
	}
	return objvalue(this) == objvalue(other);
}

static void obj_str(Value *this, Str *dest)
{
#define STR_MAX_LEN 50
	char buf[STR_MAX_LEN];
	size_t len = snprintf(buf, STR_MAX_LEN, "<%s at %p>", getclass(this)->name, objvalue(this));
	assert(len > 0);

	if (len > STR_MAX_LEN) {
		len = STR_MAX_LEN;
	}

	char *copy = rho_malloc(len + 1);
	strcpy(copy, buf);
	*dest = STR_INIT(copy, len, 1);
#undef STR_MAX_LEN
}

static bool obj_nonzero(Value *this)
{
	UNUSED(this);
	return true;
}

static void obj_free(Value *this)
{
	free(objvalue(this));
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

	NULL,    /* bitnot */
	NULL,    /* bitand */
	NULL,    /* bitor */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	NULL,    /* iadd */
	NULL,    /* isub */
	NULL,    /* imul */
	NULL,    /* idiv */
	NULL,    /* imod */
	NULL,    /* ipow */

	NULL,    /* ibitand */
	NULL,    /* ibitor */
	NULL,    /* ixor */
	NULL,    /* ishiftl */
	NULL,    /* ishiftr */

	NULL,    /* radd */
	NULL,    /* rsub */
	NULL,    /* rmul */
	NULL,    /* rdiv */
	NULL,    /* rmod */
	NULL,    /* rpow */

	NULL,    /* rbitand */
	NULL,    /* rbitor */
	NULL,    /* rxor */
	NULL,    /* rshiftl */
	NULL,    /* rshiftr */

	obj_nonzero,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct seq_methods obj_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
};

Class obj_class = {
	.base = CLASS_BASE_INIT(),
	.name = "Object",
	.super = NULL,

	.instance_size = sizeof(Object),

	.init = obj_init,
	.del = obj_free,

	.eq = obj_eq,
	.hash = NULL,
	.cmp = NULL,
	.str = obj_str,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &obj_num_methods,
	.seq_methods = &obj_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

Class *getclass(Value *v)
{
	if (v == NULL) {
		return NULL;
	}

	switch (v->type) {
	case VAL_TYPE_INT:
		return &int_class;
	case VAL_TYPE_FLOAT:
		return &float_class;
	case VAL_TYPE_OBJECT:
	case VAL_TYPE_EXC: {
		const Object *o = objvalue(v);
		return o->class;
	case VAL_TYPE_EMPTY:
	case VAL_TYPE_ERROR:
	case VAL_TYPE_UNSUPPORTED_TYPES:
	case VAL_TYPE_DIV_BY_ZERO:
		INTERNAL_ERROR();
		return NULL;
	}
	}

	INTERNAL_ERROR();
	return NULL;
}

/*
 * Generic "is a" -- checks if the given object is
 * an instance of the given class or any of its super-
 * classes. This is subject to change if support for
 * multiple-inheritance is ever added.
 */
bool is_a(Value *v, Class *class)
{
	return is_subclass(getclass(v), class);
}

bool is_subclass(Class *child, Class *parent)
{
	while (child != NULL) {
		if (child == parent) {
			return true;
		} else if (child == &meta_class) {
			return false;
		}
		child = child->super;
	}
	return false;
}

#define MAKE_METHOD_RESOLVER_DIRECT(name, type) \
type resolve_##name(Class *class) { \
	Class *target = class; \
	type op; \
	while (target != NULL && (op = target->name) == NULL) { \
		if (target == target->super) { \
			return obj_class.name; \
		} \
		target = target->super; \
	} \
\
	if (target == NULL) { \
		return obj_class.name; \
	} \
\
	class->name = op; \
	return op; \
}

#define MAKE_METHOD_RESOLVER(name, category, type) \
type resolve_##name(Class *class) { \
	if (class->category == NULL) { \
		return obj_class.category->name; \
	} \
\
	Class *target = class; \
	type op; \
	while (target != NULL && (op = target->category->name) == NULL) { \
		if (target == target->super) { \
			return obj_class.category->name; \
		} \
		target = target->super; \
	} \
\
	if (target == NULL) { \
		return obj_class.category->name; \
	} \
\
	class->category->name = op; \
	return op; \
}

/*
 * Initializers should not be inherited.
 */
InitFunc resolve_init(Class *class)
{
	return class->init;
}

/*
 * Every class should implement `del`.
 */
DelFunc resolve_del(Class *class)
{
	return class->del;
}

MAKE_METHOD_RESOLVER_DIRECT(eq, BoolBinOp)
MAKE_METHOD_RESOLVER_DIRECT(hash, IntUnOp)
MAKE_METHOD_RESOLVER_DIRECT(cmp, BinOp)
MAKE_METHOD_RESOLVER_DIRECT(str, StrUnOp)
MAKE_METHOD_RESOLVER_DIRECT(call, CallFunc)
MAKE_METHOD_RESOLVER_DIRECT(print, PrintFunc)
MAKE_METHOD_RESOLVER_DIRECT(iter, UnOp)
MAKE_METHOD_RESOLVER_DIRECT(iternext, UnOp)
MAKE_METHOD_RESOLVER_DIRECT(attr_get, AttrGetFunc)
MAKE_METHOD_RESOLVER_DIRECT(attr_set, AttrSetFunc)

MAKE_METHOD_RESOLVER(plus, num_methods, UnOp)
MAKE_METHOD_RESOLVER(minus, num_methods, UnOp)
MAKE_METHOD_RESOLVER(abs, num_methods, UnOp)
MAKE_METHOD_RESOLVER(add, num_methods, BinOp)
MAKE_METHOD_RESOLVER(sub, num_methods, BinOp)
MAKE_METHOD_RESOLVER(mul, num_methods, BinOp)
MAKE_METHOD_RESOLVER(div, num_methods, BinOp)
MAKE_METHOD_RESOLVER(mod, num_methods, BinOp)
MAKE_METHOD_RESOLVER(pow, num_methods, BinOp)
MAKE_METHOD_RESOLVER(bitnot, num_methods, UnOp)
MAKE_METHOD_RESOLVER(bitand, num_methods, BinOp)
MAKE_METHOD_RESOLVER(bitor, num_methods, BinOp)
MAKE_METHOD_RESOLVER(xor, num_methods, BinOp)
MAKE_METHOD_RESOLVER(shiftl, num_methods, BinOp)
MAKE_METHOD_RESOLVER(shiftr, num_methods, BinOp)
MAKE_METHOD_RESOLVER(iadd, num_methods, BinOp)
MAKE_METHOD_RESOLVER(isub, num_methods, BinOp)
MAKE_METHOD_RESOLVER(imul, num_methods, BinOp)
MAKE_METHOD_RESOLVER(idiv, num_methods, BinOp)
MAKE_METHOD_RESOLVER(imod, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ipow, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ibitand, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ibitor, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ixor, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ishiftl, num_methods, BinOp)
MAKE_METHOD_RESOLVER(ishiftr, num_methods, BinOp)
MAKE_METHOD_RESOLVER(radd, num_methods, BinOp)
MAKE_METHOD_RESOLVER(rsub, num_methods, BinOp)
MAKE_METHOD_RESOLVER(rmul, num_methods, BinOp)
MAKE_METHOD_RESOLVER(rdiv, num_methods, BinOp)
MAKE_METHOD_RESOLVER(rmod, num_methods, BinOp)
MAKE_METHOD_RESOLVER(rpow, num_methods, BinOp)
MAKE_METHOD_RESOLVER(rbitand, num_methods, BinOp)
MAKE_METHOD_RESOLVER(rbitor, num_methods, BinOp)
MAKE_METHOD_RESOLVER(rxor, num_methods, BinOp)
MAKE_METHOD_RESOLVER(rshiftl, num_methods, BinOp)
MAKE_METHOD_RESOLVER(rshiftr, num_methods, BinOp)
MAKE_METHOD_RESOLVER(nonzero, num_methods, BoolUnOp)
MAKE_METHOD_RESOLVER(to_int, num_methods, UnOp)
MAKE_METHOD_RESOLVER(to_float, num_methods, UnOp)

MAKE_METHOD_RESOLVER(len, seq_methods, LenFunc)
MAKE_METHOD_RESOLVER(get, seq_methods, BinOp)
MAKE_METHOD_RESOLVER(set, seq_methods, SeqSetFunc)
MAKE_METHOD_RESOLVER(contains, seq_methods, BoolBinOp)

#undef MAKE_METHOD_RESOLVER_DIRECT
#undef MAKE_METHOD_RESOLVER

void *obj_alloc(Class *class)
{
	return obj_alloc_var(class, 0);
}

void *obj_alloc_var(Class *class, size_t extra)
{
	Object *o = rho_malloc(class->instance_size + extra);
	o->class = class;
	o->refcnt = 1;
	return o;
}

Value instantiate(Class *class, Value *args, size_t nargs)
{
	if (class == &int_class) {
		return makeint(0);
	} else if (class == &float_class) {
		return makefloat(0);
	} else {
		InitFunc init = resolve_init(class);

		if (!init) {
			return type_exc_cannot_instantiate(class);
		}

		Value instance = makeobj(obj_alloc(class));
		init(&instance, args, nargs);
		return instance;
	}
}

void retaino(Object *o)
{
	if (o->refcnt != (unsigned)(-1)) {
		++o->refcnt;
	}
}

void releaseo(Object *o)
{
	if (o->refcnt != (unsigned)(-1) && --o->refcnt == 0) {
		destroyo(o);
	}
}

void destroyo(Object *o)
{
	o->class->del(&makeobj(o));
}

void retain(Value *v)
{
	if (v == NULL || !(isobject(v) || isexc(v))) {
		return;
	}
	retaino(objvalue(v));
}


void release(Value *v)
{
	if (v == NULL || !(isobject(v) || isexc(v))) {
		return;
	}
	releaseo(objvalue(v));
}

void destroy(Value *v)
{
	if (v == NULL || v->type != VAL_TYPE_OBJECT) {
		return;
	}
	Object *o = objvalue(v);
	o->class->del(v);
}

void class_init(Class *class)
{
	/* initialize attributes */
	size_t max_size = 0;

	if (class->members != NULL) {
		for (size_t i = 0; class->members[i].name != NULL; i++) {
			++max_size;
		}
	}

	if (class->methods != NULL) {
		for (struct attr_method *m = class->methods; m->name != NULL; m++) {
			++max_size;
		}
	}

	attr_dict_init(&class->attr_dict, max_size);
	attr_dict_register_members(&class->attr_dict, class->members);
	attr_dict_register_methods(&class->attr_dict, class->methods);
}
