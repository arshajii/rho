#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "exc.h"
#include "strbuf.h"
#include "vmops.h"
#include "object.h"
#include "util.h"
#include "listobject.h"

static Value iter_make(ListObject *list);

#define INDEX_CHECK(index, count) \
	if ((index) < 0 || ((size_t)(index)) >= (count)) { \
		return INDEX_EXC("list index out of range (index = %li, len = %lu)", (index), (count)); \
	}

static void list_ensure_capacity(ListObject *list, const size_t min_capacity);

/* Does not retain elements; direct transfer from value stack. */
Value list_make(Value *elements, const size_t count)
{
	ListObject *list = obj_alloc(&list_class);

	const size_t size = count * sizeof(Value);
	list->elements = rho_malloc(size);
	memcpy(list->elements, elements, size);

	list->count = count;
	list->capacity = count;

	return makeobj(list);
}

static void list_str(Value *this, Str *dest)
{
	static Str empty_list_str = STR_INIT("[]", 2, 0);

	ListObject *list = objvalue(this);
	const size_t count = list->count;

	if (count == 0) {
		*dest = empty_list_str;
		return;
	}

	StrBuf sb;
	strbuf_init(&sb, 16);
	strbuf_append(&sb, "[", 1);

	Value *elements = list->elements;

	for (size_t i = 0; i < count; i++) {
		Value *v = &elements[i];
		if (isobject(v) && objvalue(v) == list) {
			strbuf_append(&sb, "[...]", 5);
		} else {
			Str str;
			op_str(v, &str);

			strbuf_append(&sb, str.value, str.len);
			if (str.freeable) {
				str_dealloc(&str);
			}
		}

		if (i < count - 1) {
			strbuf_append(&sb, ", ", 2);
		} else {
			strbuf_append(&sb, "]", 1);
			break;
		}
	}

	strbuf_to_str(&sb, dest);
	dest->freeable = 1;
}

static void list_free(Value *this)
{
	ListObject *list = objvalue(this);
	Value *elements = list->elements;
	const size_t count = list->count;

	for (size_t i = 0; i < count; i++) {
		release(&elements[i]);
	}

	free(elements);
	obj_class.del(this);
}

static size_t list_len(Value *this)
{
	ListObject *list = objvalue(this);
	return list->count;
}

static Value list_get(Value *this, Value *idx)
{
	if (!isint(idx)) {
		Class *class = getclass(idx);
		return TYPE_EXC("list indices must be integers, not %s instances", class->name);
	}

	ListObject *list = objvalue(this);
	const size_t count = list->count;
	const long idx_raw = intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	retain(&list->elements[idx_raw]);
	return list->elements[idx_raw];
}

static Value list_set(Value *this, Value *idx, Value *v)
{
	if (!isint(idx)) {
		Class *class = getclass(idx);
		return TYPE_EXC("list indices must be integers, not %s instances", class->name);
	}

	ListObject *list = objvalue(this);
	const size_t count = list->count;
	const long idx_raw = intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	Value old = list->elements[idx_raw];
	retain(v);
	list->elements[idx_raw] = *v;
	return old;
}

#define NAMED_ARGS_CHECK(name) \
		if (nargs_named > 0) return TYPE_EXC(name "() takes no named arguments")

static Value list_append(Value *this,
                         Value *args,
                         Value *args_named,
                         size_t nargs,
                         size_t nargs_named)
{
#define NAME "append"

	UNUSED(args_named);
	NAMED_ARGS_CHECK(NAME);

	if (nargs != 1) {
		return TYPE_EXC(NAME "() takes exactly 1 argument (got %lu)", nargs);
	}

	ListObject *list = objvalue(this);
	const size_t count = list->count;
	list_ensure_capacity(list, count + 1);

	retain(&args[0]);
	list->elements[list->count++] = args[0];

	return makeint(0);

#undef NAME
}

static Value list_pop(Value *this,
                      Value *args,
                      Value *args_named,
                      size_t nargs,
                      size_t nargs_named)
{
#define NAME "pop"

	UNUSED(args_named);
	NAMED_ARGS_CHECK(NAME);

	if (nargs >= 2) {
		return TYPE_EXC(NAME "() takes at most 1 argument (got %lu)", nargs);
	}

	ListObject *list = objvalue(this);
	Value *elements = list->elements;
	const size_t count = list->count;

	if (nargs == 0) {
		if (count > 0) {
			return elements[--list->count];
		} else {
			return INDEX_EXC("cannot invoke " NAME "() on an empty list");
		}
	} else {
		Value *idx = &args[0];
		if (isint(idx)) {
			const long idx_raw = intvalue(idx);

			INDEX_CHECK(idx_raw, count);

			Value ret = elements[idx_raw];
			memmove(&elements[idx_raw],
			        &elements[idx_raw + 1],
			        ((count - 1) - idx_raw) * sizeof(Value));
			--list->count;
			return ret;
		} else {
			Class *class = getclass(idx);
			return TYPE_EXC(NAME "() requires an integer argument (got a %s)", class->name);
		}
	}

#undef NAME
}

static Value list_insert(Value *this,
                         Value *args,
                         Value *args_named,
                         size_t nargs,
                         size_t nargs_named)
{
#define NAME "insert"

	UNUSED(args_named);
	NAMED_ARGS_CHECK(NAME);

	if (nargs != 2) {
		return TYPE_EXC(NAME "() takes exactly 1 argument (got %lu)", nargs);
	}

	ListObject *list = objvalue(this);
	const size_t count = list->count;

	Value *idx = &args[0];
	Value *e = &args[1];

	if (!isint(idx)) {
		Class *class = getclass(idx);
		return TYPE_EXC(NAME "() requires an integer as its first argument (got a %s)", class->name);
	}

	const long idx_raw = intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	list_ensure_capacity(list, count + 1);
	Value *elements = list->elements;

	memmove(&elements[idx_raw + 1],
	        &elements[idx_raw],
	        (count - idx_raw) * sizeof(Value));

	retain(e);
	elements[idx_raw] = *e;
	++list->count;
	return makeint(0);

#undef NAME
}

static Value list_iter(Value *this)
{
	ListObject *list = objvalue(this);
	return iter_make(list);
}

static void list_ensure_capacity(ListObject *list, const size_t min_capacity)
{
	const size_t capacity = list->capacity;

	if (capacity < min_capacity) {
		size_t new_capacity = (capacity * 3)/2 + 1;

		if (new_capacity < min_capacity) {
			new_capacity = min_capacity;
		}

		list->elements = rho_realloc(list->elements, new_capacity * sizeof(Value));
		list->capacity = new_capacity;
	}
}

struct num_methods list_num_methods = {
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

struct seq_methods list_seq_methods = {
	list_len,    /* len */
	list_get,    /* get */
	list_set,    /* set */
	NULL,    /* contains */
};

struct attr_method list_methods[] = {
		{"append", list_append},
		{"pop", list_pop},
		{"insert", list_insert},
		{NULL, NULL}
};

Class list_class = {
	.base = CLASS_BASE_INIT(),
	.name = "List",
	.super = &obj_class,

	.instance_size = sizeof(ListObject),

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

	.num_methods = &list_num_methods,
	.seq_methods = &list_seq_methods,

	.members = NULL,
	.methods = list_methods,

	.attr_get = NULL,
	.attr_set = NULL
};


/* list iterator */

static Value iter_make(ListObject *list)
{
	ListIter *iter = obj_alloc(&list_iter_class);
	retaino((Object *)list);
	iter->source = list;
	iter->index = 0;
	return makeobj(iter);
}

static Value iter_next(Value *this)
{
	ListIter *iter = objvalue(this);
	if (iter->index == iter->source->count) {
		return get_iter_stop();
	} else {
		Value v = iter->source->elements[iter->index++];
		retain(&v);
		return v;
	}
}

static void iter_free(Value *this)
{
	ListIter *iter = objvalue(this);
	releaseo((Object *)iter->source);
	iter_class.del(this);
}

Class list_iter_class = {
	.base = CLASS_BASE_INIT(),
	.name = "ListIter",
	.super = &iter_class,

	.instance_size = sizeof(ListIter),

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
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
