#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "exc.h"
#include "strbuf.h"
#include "vmops.h"
#include "object.h"
#include "strobject.h"
#include "tupleobject.h"

#define INDEX_CHECK(index, count) \
	if ((index) < 0 || ((size_t)(index)) >= (count)) { \
		return INDEX_EXC("tuple index out of range (index = %li, len = %lu)", (index), (count)); \
	}

/* Does not retain elements; direct transfer from value stack. */
Value tuple_make(Value *elements, const size_t count)
{
	const size_t extra_size = count * sizeof(Value);
	TupleObject *tup = obj_alloc_var(&tuple_class, extra_size);
	memcpy(tup->elements, elements, extra_size);
	tup->count = count;
	return makeobj(tup);
}

static StrObject *tuple_str(Value *this)
{
	TupleObject *tup = objvalue(this);
	const size_t count = tup->count;

	if (count == 0) {
		Value ret = strobj_make_direct("()", 2);
		return (StrObject *)objvalue(&ret);
	}

	StrBuf sb;
	strbuf_init(&sb, 16);
	strbuf_append(&sb, "(", 1);

	Value *elements = tup->elements;

	for (size_t i = 0; i < count; i++) {
		Value *v = &elements[i];
		if (isobject(v) && objvalue(v) == tup) {  // this should really never happen
			strbuf_append(&sb, "(...)", 5);
		} else {
			StrObject *str = op_str(v);
			strbuf_append(&sb, str->str.value, str->str.len);
			releaseo(str);
		}

		if (i < count - 1) {
			strbuf_append(&sb, ", ", 2);
		} else {
			strbuf_append(&sb, ")", 1);
			break;
		}
	}

	Str dest;
	strbuf_to_str(&sb, &dest);
	dest.freeable = 1;

	Value ret = strobj_make(dest);
	return (StrObject *)objvalue(&ret);
}

static void tuple_free(Value *this)
{
	TupleObject *tup = objvalue(this);
	Value *elements = tup->elements;
	const size_t count = tup->count;

	for (size_t i = 0; i < count; i++) {
		release(&elements[i]);
	}

	obj_class.del(this);
}

static size_t tuple_len(Value *this)
{
	TupleObject *tup = objvalue(this);
	return tup->count;
}

static Value tuple_get(Value *this, Value *idx)
{
	if (!isint(idx)) {
		return TYPE_EXC("list indices must be integers, not %s instances", getclass(idx)->name);
	}

	TupleObject *tup = objvalue(this);
	const size_t count = tup->count;
	const long idx_raw = intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	retain(&tup->elements[idx_raw]);
	return tup->elements[idx_raw];
}

struct num_methods tuple_num_methods = {
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

struct seq_methods tuple_seq_methods = {
	tuple_len,    /* len */
	tuple_get,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

Class tuple_class = {
	.base = CLASS_BASE_INIT(),
	.name = "Tuple",
	.super = &obj_class,

	.instance_size = sizeof(TupleObject),  /* variable-length */

	.init = NULL,
	.del = tuple_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = tuple_str,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &tuple_num_methods,
	.seq_methods  = &tuple_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
