#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "attr.h"
#include "exc.h"
#include "err.h"
#include "util.h"
#include "null.h"
#include "boolobject.h"
#include "intobject.h"
#include "floatobject.h"
#include "strobject.h"
#include "exc.h"
#include "object.h"

static RhoValue obj_init(RhoValue *this, RhoValue *args, size_t nargs)
{
	RHO_UNUSED(args);

	if (nargs > 0) {
		return RHO_TYPE_EXC("Object constructor takes no arguments (got %lu)", nargs);
	}

	return *this;
}

static RhoValue obj_eq(RhoValue *this, RhoValue *other)
{
	if (!rho_isobject(other)) {
		return rho_makefalse();
	}
	return rho_makebool(rho_objvalue(this) == rho_objvalue(other));
}

static RhoValue obj_str(RhoValue *this)
{
#define STR_MAX_LEN 50
	char buf[STR_MAX_LEN];
	size_t len = snprintf(buf, STR_MAX_LEN, "<%s at %p>", rho_getclass(this)->name, rho_objvalue(this));
	assert(len > 0);

	if (len > STR_MAX_LEN) {
		len = STR_MAX_LEN;
	}

	return rho_strobj_make_direct(buf, len);
#undef STR_MAX_LEN
}

static bool obj_nonzero(RhoValue *this)
{
	RHO_UNUSED(this);
	return true;
}

static void obj_free(RhoValue *this)
{
	free(rho_objvalue(this));
}

struct rho_num_methods obj_num_methods = {
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

struct rho_seq_methods obj_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_obj_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Object",
	.super = NULL,

	.instance_size = sizeof(RhoObject),

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

RhoClass *rho_getclass(RhoValue *v)
{
	if (v == NULL) {
		return NULL;
	}

	switch (v->type) {
	case RHO_VAL_TYPE_NULL:
		return &rho_null_class;
	case RHO_VAL_TYPE_BOOL:
		return &rho_bool_class;
	case RHO_VAL_TYPE_INT:
		return &rho_int_class;
	case RHO_VAL_TYPE_FLOAT:
		return &rho_float_class;
	case RHO_VAL_TYPE_OBJECT:
	case RHO_VAL_TYPE_EXC: {
		const RhoObject *o = rho_objvalue(v);
		return o->class;
	case RHO_VAL_TYPE_EMPTY:
	case RHO_VAL_TYPE_ERROR:
	case RHO_VAL_TYPE_UNSUPPORTED_TYPES:
	case RHO_VAL_TYPE_DIV_BY_ZERO:
		RHO_INTERNAL_ERROR();
		return NULL;
	}
	}

	RHO_INTERNAL_ERROR();
	return NULL;
}

/*
 * Generic "is a" -- checks if the given object is
 * an instance of the given class or any of its super-
 * classes. This is subject to change if support for
 * multiple-inheritance is ever added.
 */
bool rho_is_a(RhoValue *v, RhoClass *class)
{
	return rho_is_subclass(rho_getclass(v), class);
}

bool rho_is_subclass(RhoClass *child, RhoClass *parent)
{
	while (child != NULL) {
		if (child == parent) {
			return true;
		} else if (child == &rho_meta_class) {
			return false;
		}
		child = child->super;
	}
	return false;
}

#define MAKE_METHOD_RESOLVER_DIRECT(name, type) \
type rho_resolve_##name(RhoClass *class) { \
	RhoClass *target = class; \
	type op; \
	while (target != NULL && (op = target->name) == NULL) { \
		if (target == target->super) { \
			return rho_obj_class.name; \
		} \
		target = target->super; \
	} \
\
	if (target == NULL) { \
		return rho_obj_class.name; \
	} \
\
	class->name = op; \
	return op; \
}

#define MAKE_METHOD_RESOLVER(name, category, type) \
type rho_resolve_##name(RhoClass *class) { \
	if (class->category == NULL) { \
		return rho_obj_class.category->name; \
	} \
\
	RhoClass *target = class; \
	type op; \
	while (target != NULL && (op = target->category->name) == NULL) { \
		if (target == target->super) { \
			return rho_obj_class.category->name; \
		} \
		target = target->super; \
	} \
\
	if (target == NULL) { \
		return rho_obj_class.category->name; \
	} \
\
	class->category->name = op; \
	return op; \
}

/*
 * Initializers should not be inherited.
 */
RhoInitFunc rho_resolve_init(RhoClass *class)
{
	return class->init;
}

/*
 * Every class should implement `del`.
 */
RhoDelFunc rho_resolve_del(RhoClass *class)
{
	return class->del;
}

MAKE_METHOD_RESOLVER_DIRECT(eq, RhoBinOp)
MAKE_METHOD_RESOLVER_DIRECT(hash, RhoUnOp)
MAKE_METHOD_RESOLVER_DIRECT(cmp, RhoBinOp)
MAKE_METHOD_RESOLVER_DIRECT(str, RhoUnOp)
MAKE_METHOD_RESOLVER_DIRECT(call, RhoCallFunc)
MAKE_METHOD_RESOLVER_DIRECT(print, RhoPrintFunc)
MAKE_METHOD_RESOLVER_DIRECT(iter, RhoUnOp)
MAKE_METHOD_RESOLVER_DIRECT(iternext, RhoUnOp)
MAKE_METHOD_RESOLVER_DIRECT(attr_get, RhoAttrGetFunc)
MAKE_METHOD_RESOLVER_DIRECT(attr_set, RhoAttrSetFunc)

MAKE_METHOD_RESOLVER(plus, num_methods, RhoUnOp)
MAKE_METHOD_RESOLVER(minus, num_methods, RhoUnOp)
MAKE_METHOD_RESOLVER(abs, num_methods, RhoUnOp)
MAKE_METHOD_RESOLVER(add, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(sub, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(mul, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(div, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(mod, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(pow, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(bitnot, num_methods, RhoUnOp)
MAKE_METHOD_RESOLVER(bitand, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(bitor, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(xor, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(shiftl, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(shiftr, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(iadd, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(isub, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(imul, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(idiv, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(imod, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(ipow, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(ibitand, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(ibitor, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(ixor, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(ishiftl, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(ishiftr, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(radd, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(rsub, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(rmul, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(rdiv, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(rmod, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(rpow, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(rbitand, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(rbitor, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(rxor, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(rshiftl, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(rshiftr, num_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(nonzero, num_methods, BoolUnOp)
MAKE_METHOD_RESOLVER(to_int, num_methods, RhoUnOp)
MAKE_METHOD_RESOLVER(to_float, num_methods, RhoUnOp)

MAKE_METHOD_RESOLVER(len, seq_methods, RhoUnOp)
MAKE_METHOD_RESOLVER(get, seq_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(set, seq_methods, RhoSeqSetFunc)
MAKE_METHOD_RESOLVER(contains, seq_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(apply, seq_methods, RhoBinOp)
MAKE_METHOD_RESOLVER(iapply, seq_methods, RhoBinOp)

#undef MAKE_METHOD_RESOLVER_DIRECT
#undef MAKE_METHOD_RESOLVER

void *rho_obj_alloc(RhoClass *class)
{
	return rho_obj_alloc_var(class, 0);
}

void *rho_obj_alloc_var(RhoClass *class, size_t extra)
{
	RhoObject *o = rho_malloc(class->instance_size + extra);
	o->class = class;
	o->refcnt = 1;
	return o;
}

RhoValue rho_class_instantiate(RhoClass *class, RhoValue *args, size_t nargs)
{
	if (class == &rho_null_class) {
		return rho_makenull();
	} else if (class == &rho_int_class) {
		return rho_makefalse();
	} else if (class == &rho_float_class) {
		return rho_makefloat(0);
	} else {
		RhoInitFunc init = rho_resolve_init(class);

		if (!init) {
			return rho_type_exc_cannot_instantiate(class);
		}

		RhoValue instance = rho_makeobj(rho_obj_alloc(class));
		init(&instance, args, nargs);
		return instance;
	}
}

void rho_retaino(void *p)
{
	RhoObject *o = p;
	if (o->refcnt != (unsigned)(-1)) {
		++o->refcnt;
	}
}

void rho_releaseo(void *p)
{
	RhoObject *o = p;
	if (o->refcnt != (unsigned)(-1) && --o->refcnt == 0) {
		rho_destroyo(o);
	}
}

void rho_destroyo(void *p)
{
	RhoObject *o = p;
	o->class->del(&rho_makeobj(o));
}

void rho_retain(RhoValue *v)
{
	if (v == NULL || !(rho_isobject(v) || rho_isexc(v))) {
		return;
	}
	rho_retaino(rho_objvalue(v));
}

void rho_release(RhoValue *v)
{
	if (v == NULL || !(rho_isobject(v) || rho_isexc(v))) {
		return;
	}
	rho_releaseo(rho_objvalue(v));
}

void rho_destroy(RhoValue *v)
{
	if (v == NULL || v->type != RHO_VAL_TYPE_OBJECT) {
		return;
	}
	RhoObject *o = rho_objvalue(v);
	o->class->del(v);
}

void rho_class_init(RhoClass *class)
{
	/* initialize attributes */
	size_t max_size = 0;

	if (class->members != NULL) {
		for (size_t i = 0; class->members[i].name != NULL; i++) {
			++max_size;
		}
	}

	if (class->methods != NULL) {
		for (struct rho_attr_method *m = class->methods; m->name != NULL; m++) {
			++max_size;
		}
	}

	rho_attr_dict_init(&class->attr_dict, max_size);
	rho_attr_dict_register_members(&class->attr_dict, class->members);
	rho_attr_dict_register_methods(&class->attr_dict, class->methods);
}
