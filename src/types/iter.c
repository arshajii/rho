#include <stdlib.h>
#include <stdbool.h>
#include "object.h"
#include "exc.h"
#include "iter.h"

/* Base Iter */
static void iter_free(RhoValue *this)
{
	rho_obj_class.del(this);
}

static RhoValue iter_iter(RhoValue *this)
{
	rho_retain(this);
	return *this;
}

static RhoValue iter_apply(RhoValue *this, RhoValue *fn)
{
	RhoIter *iter = rho_objvalue(this);
	return rho_applied_iter_make(iter, fn);
}

struct rho_seq_methods iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	iter_apply,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_iter_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Iter",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoIter),

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
RhoValue rho_get_iter_stop(void)
{
	static RhoIterStop iter_stop = { .base = RHO_OBJ_INIT_STATIC(&rho_iter_stop_class) };
	return rho_makeobj(&iter_stop);
}

static void iter_stop_free(RhoValue *this)
{
	rho_obj_class.del(this);
}

RhoClass rho_iter_stop_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "IterStop",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoIterStop),

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
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* AppliedIter -- result of for e.g. `function @ iter` */
RhoValue rho_applied_iter_make(RhoIter *source, RhoValue *fn)
{
	RhoAppliedIter *appiter = rho_obj_alloc(&rho_applied_iter_class);
	rho_retaino(source);
	rho_retain(fn);
	appiter->source = source;
	appiter->fn = *fn;
	return rho_makeobj(appiter);
}

static void applied_iter_free(RhoValue *this)
{
	RhoAppliedIter *appiter = rho_objvalue(this);
	rho_releaseo(appiter->source);
	rho_release(&appiter->fn);
	rho_iter_class.del(this);
}

static RhoValue applied_iter_iternext(RhoValue *this)
{
	RhoAppliedIter *appiter = rho_objvalue(this);
	RhoIter *source = appiter->source;
	RhoClass *source_class = source->base.class;
	UnOp iternext = rho_resolve_iternext(source_class);

	if (!iternext) {
		return rho_type_exc_not_iterator(source_class);
	}

	RhoValue *fn = &appiter->fn;
	RhoClass *fn_class = rho_getclass(fn);
	CallFunc call = rho_resolve_call(fn_class);

	if (!call) {
		return rho_type_exc_not_callable(fn_class);
	}

	RhoValue next = iternext(&rho_makeobj(source));

	if (rho_is_iter_stop(&next) || rho_iserror(&next)) {
		return next;
	}

	RhoValue ret = call(fn, &next, NULL, 1, 0);
	rho_release(&next);
	return ret;
}

struct rho_seq_methods applied_iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_applied_iter_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "AppliedIter",
	.super = &rho_iter_class,

	.instance_size = sizeof(RhoAppliedIter),

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
	.seq_methods = &applied_iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* Range iterators */
RhoValue rho_range_make(RhoValue *from, RhoValue *to)
{
	if (!(rho_isint(from) && rho_isint(to))) {
		return rho_type_exc_unsupported_2("..", rho_getclass(from), rho_getclass(to));
	}

	RhoRange *range = rho_obj_alloc(&rho_range_class);
	range->from = range->i = rho_intvalue(from);
	range->to = rho_intvalue(to);
	return rho_makeobj(range);
}

static void range_free(RhoValue *this)
{
	rho_iter_class.del(this);
}

static RhoValue range_iternext(RhoValue *this)
{
	RhoRange *range = rho_objvalue(this);

	const long from = range->from;
	const long to = range->to;
	const long i = range->i;

	if (to >= from) {
		if (i < to) {
			++(range->i);
			return rho_makeint(i);
		} else {
			return rho_get_iter_stop();
		}
	} else {
		if (i >= to) {
			--(range->i);
			return rho_makeint(i);
		} else {
			return rho_get_iter_stop();
		}
	}
}

static bool range_contains(RhoValue *this, RhoValue *n)
{
	if (!rho_isint(n)) {
		return false;
	}

	RhoRange *range = rho_objvalue(this);
	const long target = rho_intvalue(n);
	const long from = range->from;
	const long to = range->to;

	if (to >= from) {
		return from <= target && target < to;
	} else {
		return to <= target && target <= from;
	}
}

struct rho_seq_methods range_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	range_contains,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_range_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Range",
	.super = &rho_iter_class,

	.instance_size = sizeof(RhoRange),

	.init = NULL,
	.del = range_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = range_iternext,

	.num_methods = NULL,
	.seq_methods = &range_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
