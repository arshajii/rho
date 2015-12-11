#include <stdlib.h>
#include "object.h"
#include "exc.h"
#include "iter.h"

/* Base Iter */
static void iter_free(Value *this)
{
	obj_class.del(this);
}

static Value iter_iter(Value *this)
{
	retain(this);
	return *this;
}

static Value iter_apply(Value *this, Value *fn)
{
	Iter *iter = objvalue(this);
	return applied_iter_make(iter, fn);
}

struct seq_methods iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	iter_apply,    /* apply */
	NULL,    /* iapply */
};

Class iter_class = {
	.base = CLASS_BASE_INIT(),
	.name = "Iter",
	.super = &obj_class,

	.instance_size = sizeof(Iter),

	.init = NULL,
	.del = iter_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = iter_iter,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = &iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* IterStop */
Value get_iter_stop(void)
{
	static IterStop iter_stop = (IterStop){OBJ_INIT_STATIC(&iter_stop_class)};
	return makeobj(&iter_stop);
}

static void iter_stop_free(Value *this)
{
	obj_class.del(this);
}

Class iter_stop_class = {
	.base = CLASS_BASE_INIT(),
	.name = "IterStop",
	.super = &obj_class,

	.instance_size = sizeof(IterStop),

	.init = NULL,
	.del = iter_stop_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* AppliedIter -- result of for e.g. `function @ iter` */
Value applied_iter_make(Iter *source, Value *fn)
{
	AppliedIter *appiter = obj_alloc(&applied_iter_class);
	retaino((Object *)source);
	retain(fn);
	appiter->source = source;
	appiter->fn = *fn;
	return makeobj(appiter);
}

static void applied_iter_free(Value *this)
{
	AppliedIter *appiter = objvalue(this);
	releaseo((Object *)appiter->source);
	release(&appiter->fn);
	iter_class.del(this);
}

static Value applied_iter_iternext(Value *this)
{
	AppliedIter *appiter = objvalue(this);
	Iter *source = appiter->source;
	Class *source_class = source->base.class;
	UnOp iternext = resolve_iternext(source_class);

	if (!iternext) {
		return type_exc_not_iterator(source_class);
	}

	Value *fn = &appiter->fn;
	Class *fn_class = getclass(fn);
	CallFunc call = resolve_call(fn_class);

	if (!call) {
		return type_exc_not_callable(fn_class);
	}

	Value next = iternext(&makeobj(source));

	if (is_iter_stop(&next) || iserror(&next)) {
		return next;
	}

	Value ret = call(fn, &next, NULL, 1, 0);
	release(&next);
	return ret;
}

struct seq_methods applied_iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

Class applied_iter_class = {
	.base = CLASS_BASE_INIT(),
	.name = "AppliedIter",
	.super = &iter_class,

	.instance_size = sizeof(AppliedIter),

	.init = NULL,
	.del = applied_iter_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = applied_iter_iternext,

	.num_methods = NULL,
	.seq_methods  = &applied_iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
