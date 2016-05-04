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
		return RHO_INDEX_EXC("tuple index out of range (index = %li, len = %lu)", (index), (count)); \
	}

/* Does not retain elements; direct transfer from value stack. */
RhoValue rho_tuple_make(RhoValue *elements, const size_t count)
{
	const size_t extra_size = count * sizeof(RhoValue);
	RhoTupleObject *tup = rho_obj_alloc_var(&rho_tuple_class, extra_size);
	memcpy(tup->elements, elements, extra_size);
	tup->count = count;
	return rho_makeobj(tup);
}

static RhoStrObject *tuple_str(RhoValue *this)
{
	RhoTupleObject *tup = rho_objvalue(this);
	const size_t count = tup->count;

	if (count == 0) {
		RhoValue ret = rho_strobj_make_direct("()", 2);
		return (RhoStrObject *)rho_objvalue(&ret);
	}

	RhoStrBuf sb;
	rho_strbuf_init(&sb, 16);
	rho_strbuf_append(&sb, "(", 1);

	RhoValue *elements = tup->elements;

	for (size_t i = 0; i < count; i++) {
		RhoValue *v = &elements[i];
		if (rho_isobject(v) && rho_objvalue(v) == tup) {  // this should really never happen
			rho_strbuf_append(&sb, "(...)", 5);
		} else {
			RhoStrObject *str = rho_op_str(v);
			rho_strbuf_append(&sb, str->str.value, str->str.len);
			rho_releaseo(str);
		}

		if (i < count - 1) {
			rho_strbuf_append(&sb, ", ", 2);
		} else {
			rho_strbuf_append(&sb, ")", 1);
			break;
		}
	}

	RhoStr dest;
	rho_strbuf_to_str(&sb, &dest);
	dest.freeable = 1;

	RhoValue ret = rho_strobj_make(dest);
	return (RhoStrObject *)rho_objvalue(&ret);
}

static void tuple_free(RhoValue *this)
{
	RhoTupleObject *tup = rho_objvalue(this);
	RhoValue *elements = tup->elements;
	const size_t count = tup->count;

	for (size_t i = 0; i < count; i++) {
		rho_release(&elements[i]);
	}

	obj_class.del(this);
}

static size_t tuple_len(RhoValue *this)
{
	RhoTupleObject *tup = rho_objvalue(this);
	return tup->count;
}

static RhoValue tuple_get(RhoValue *this, RhoValue *idx)
{
	if (!rho_isint(idx)) {
		return RHO_TYPE_EXC("list indices must be integers, not %s instances", rho_getclass(idx)->name);
	}

	RhoTupleObject *tup = rho_objvalue(this);
	const size_t count = tup->count;
	const long idx_raw = rho_intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	rho_retain(&tup->elements[idx_raw]);
	return tup->elements[idx_raw];
}

struct rho_num_methods rho_tuple_num_methods = {
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

struct rho_seq_methods rho_tuple_seq_methods = {
	tuple_len,    /* len */
	tuple_get,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_tuple_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Tuple",
	.super = &obj_class,

	.instance_size = sizeof(RhoTupleObject),  /* variable-length */

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

	.num_methods = &rho_tuple_num_methods,
	.seq_methods = &rho_tuple_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
