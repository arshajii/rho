#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "exc.h"
#include "strbuf.h"
#include "vmops.h"
#include "object.h"
#include "strobject.h"
#include "util.h"
#include "listobject.h"

static RhoValue iter_make(RhoListObject *list);

#define INDEX_CHECK(index, count) \
	if ((index) < 0 || ((size_t)(index)) >= (count)) { \
		return RHO_INDEX_EXC("list index out of range (index = %li, len = %lu)", (index), (count)); \
	}

static void list_ensure_capacity(RhoListObject *list, const size_t min_capacity);

/* Does not retain elements; direct transfer from value stack. */
RhoValue rho_list_make(RhoValue *elements, const size_t count)
{
	RhoListObject *list = rho_obj_alloc(&rho_list_class);

	const size_t size = count * sizeof(RhoValue);
	list->elements = rho_malloc(size);
	memcpy(list->elements, elements, size);

	list->count = count;
	list->capacity = count;

	return rho_makeobj(list);
}

static RhoStrObject *list_str(RhoValue *this)
{
	RhoListObject *list = rho_objvalue(this);
	const size_t count = list->count;

	if (count == 0) {
		RhoValue ret = rho_strobj_make_direct("[]", 2);
		return (RhoStrObject *)rho_objvalue(&ret);
	}

	RhoStrBuf sb;
	rho_strbuf_init(&sb, 16);
	rho_strbuf_append(&sb, "[", 1);

	RhoValue *elements = list->elements;

	for (size_t i = 0; i < count; i++) {
		RhoValue *v = &elements[i];
		if (rho_isobject(v) && rho_objvalue(v) == list) {
			rho_strbuf_append(&sb, "[...]", 5);
		} else {
			RhoStrObject *str = rho_op_str(v);
			rho_strbuf_append(&sb, str->str.value, str->str.len);
			rho_releaseo(str);
		}

		if (i < count - 1) {
			rho_strbuf_append(&sb, ", ", 2);
		} else {
			rho_strbuf_append(&sb, "]", 1);
			break;
		}
	}

	RhoStr dest;
	rho_strbuf_to_str(&sb, &dest);
	dest.freeable = 1;

	RhoValue ret = rho_strobj_make(dest);
	return (RhoStrObject *)rho_objvalue(&ret);
}

static void list_free(RhoValue *this)
{
	RhoListObject *list = rho_objvalue(this);
	RhoValue *elements = list->elements;
	const size_t count = list->count;

	for (size_t i = 0; i < count; i++) {
		rho_release(&elements[i]);
	}

	free(elements);
	obj_class.del(this);
}

static size_t list_len(RhoValue *this)
{
	RhoListObject *list = rho_objvalue(this);
	return list->count;
}

RhoValue rho_list_get(RhoListObject *list, const size_t idx)
{
	RhoValue v = list->elements[idx];
	rho_retain(&v);
	return v;
}

static RhoValue list_get(RhoValue *this, RhoValue *idx)
{
	if (!rho_isint(idx)) {
		RhoClass *class = rho_getclass(idx);
		return RHO_TYPE_EXC("list indices must be integers, not %s instances", class->name);
	}

	RhoListObject *list = rho_objvalue(this);
	const size_t count = list->count;
	const long idx_raw = rho_intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	return rho_list_get(list, idx_raw);
}

static RhoValue list_set(RhoValue *this, RhoValue *idx, RhoValue *v)
{
	if (!rho_isint(idx)) {
		RhoClass *class = rho_getclass(idx);
		return RHO_TYPE_EXC("list indices must be integers, not %s instances", class->name);
	}

	RhoListObject *list = rho_objvalue(this);
	const size_t count = list->count;
	const long idx_raw = rho_intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	RhoValue old = list->elements[idx_raw];
	rho_retain(v);
	list->elements[idx_raw] = *v;
	return old;
}

static RhoValue list_apply(RhoValue *this, RhoValue *fn)
{
	RhoListObject *list = rho_objvalue(this);
	CallFunc call = rho_resolve_call(rho_getclass(fn));  // this should've been checked already

	RhoValue list2_value = rho_list_make(list->elements, list->count);
	RhoListObject *list2 = rho_objvalue(&list2_value);
	RhoValue *elements = list2->elements;
	const size_t count = list2->count;

	for (size_t i = 0; i < count; i++) {
		RhoValue r = call(fn, &elements[i], NULL, 1, 0);

		if (rho_iserror(&r)) {
			list2->count = i;
			list_free(&rho_makeobj(list2));
			return r;
		}

		elements[i] = r;
	}

	return rho_makeobj(list2);
}

void rho_list_append(RhoListObject *list, RhoValue *v)
{
	const size_t count = list->count;
	list_ensure_capacity(list, count + 1);
	rho_retain(v);
	list->elements[list->count++] = *v;
}

static RhoValue list_append(RhoValue *this,
                            RhoValue *args,
                            RhoValue *args_named,
                            size_t nargs,
                            size_t nargs_named)
{
#define NAME "append"

	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 1);

	RhoListObject *list = rho_objvalue(this);
	rho_list_append(list, &args[0]);
	return rho_makenull();

#undef NAME
}

static RhoValue list_pop(RhoValue *this,
                         RhoValue *args,
                         RhoValue *args_named,
                         size_t nargs,
                         size_t nargs_named)
{
#define NAME "pop"

	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK_AT_MOST(NAME, nargs, 1);

	RhoListObject *list = rho_objvalue(this);
	RhoValue *elements = list->elements;
	const size_t count = list->count;

	if (nargs == 0) {
		if (count > 0) {
			return elements[--list->count];
		} else {
			return RHO_INDEX_EXC("cannot invoke " NAME "() on an empty list");
		}
	} else {
		RhoValue *idx = &args[0];
		if (rho_isint(idx)) {
			const long idx_raw = rho_intvalue(idx);

			INDEX_CHECK(idx_raw, count);

			RhoValue ret = elements[idx_raw];
			memmove(&elements[idx_raw],
			        &elements[idx_raw + 1],
			        ((count - 1) - idx_raw) * sizeof(RhoValue));
			--list->count;
			return ret;
		} else {
			RhoClass *class = rho_getclass(idx);
			return RHO_TYPE_EXC(NAME "() requires an integer argument (got a %s)", class->name);
		}
	}

#undef NAME
}

static RhoValue list_insert(RhoValue *this,
                            RhoValue *args,
                            RhoValue *args_named,
                            size_t nargs,
                            size_t nargs_named)
{
#define NAME "insert"

	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 2);

	RhoListObject *list = rho_objvalue(this);
	const size_t count = list->count;

	RhoValue *idx = &args[0];
	RhoValue *e = &args[1];

	if (!rho_isint(idx)) {
		RhoClass *class = rho_getclass(idx);
		return RHO_TYPE_EXC(NAME "() requires an integer as its first argument (got a %s)", class->name);
	}

	const long idx_raw = rho_intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	list_ensure_capacity(list, count + 1);
	RhoValue *elements = list->elements;

	memmove(&elements[idx_raw + 1],
	        &elements[idx_raw],
	        (count - idx_raw) * sizeof(RhoValue));

	rho_retain(e);
	elements[idx_raw] = *e;
	++list->count;
	return rho_makenull();

#undef NAME
}

static RhoValue list_iter(RhoValue *this)
{
	RhoListObject *list = rho_objvalue(this);
	return iter_make(list);
}

static void list_ensure_capacity(RhoListObject *list, const size_t min_capacity)
{
	const size_t capacity = list->capacity;

	if (capacity < min_capacity) {
		size_t new_capacity = (capacity * 3)/2 + 1;

		if (new_capacity < min_capacity) {
			new_capacity = min_capacity;
		}

		list->elements = rho_realloc(list->elements, new_capacity * sizeof(RhoValue));
		list->capacity = new_capacity;
	}
}

void rho_list_clear(RhoListObject *list)
{
	RhoValue *elements = list->elements;
	const size_t count = list->count;

	for (size_t i = 0; i < count; i++) {
		rho_release(&elements[i]);
	}

	list->count = 0;
}

void rho_list_trim(RhoListObject *list)
{
	const size_t new_capacity = list->count;
	list->capacity = new_capacity;
	list->elements = rho_realloc(list->elements, new_capacity * sizeof(RhoValue));
}

struct rho_num_methods rho_list_num_methods = {
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

	NULL,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct rho_seq_methods rho_list_seq_methods = {
	list_len,    /* len */
	list_get,    /* get */
	list_set,    /* set */
	NULL,    /* contains */
	list_apply,    /* apply */
	NULL,    /* iapply */
};

struct rho_attr_method list_methods[] = {
	{"append", list_append},
	{"pop", list_pop},
	{"insert", list_insert},
	{NULL, NULL}
};

RhoClass rho_list_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "List",
	.super = &obj_class,

	.instance_size = sizeof(RhoListObject),

	.init = NULL,
	.del = list_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = list_str,
	.call = NULL,

	.print = NULL,

	.iter = list_iter,
	.iternext = NULL,

	.num_methods = &rho_list_num_methods,
	.seq_methods = &rho_list_seq_methods,

	.members = NULL,
	.methods = list_methods,

	.attr_get = NULL,
	.attr_set = NULL
};


/* list iterator */

static RhoValue iter_make(RhoListObject *list)
{
	RhoListIter *iter = rho_obj_alloc(&rho_list_iter_class);
	rho_retaino(list);
	iter->source = list;
	iter->index = 0;
	return rho_makeobj(iter);
}

static RhoValue iter_next(RhoValue *this)
{
	RhoListIter *iter = rho_objvalue(this);
	if (iter->index >= iter->source->count) {
		return rho_get_iter_stop();
	} else {
		RhoValue v = iter->source->elements[iter->index++];
		rho_retain(&v);
		return v;
	}
}

static void iter_free(RhoValue *this)
{
	RhoListIter *iter = rho_objvalue(this);
	rho_releaseo(iter->source);
	rho_iter_class.del(this);
}

struct rho_seq_methods list_iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_list_iter_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "ListIter",
	.super = &rho_iter_class,

	.instance_size = sizeof(RhoListIter),

	.init = NULL,
	.del = iter_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = iter_next,

	.num_methods = NULL,
	.seq_methods = &list_iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
