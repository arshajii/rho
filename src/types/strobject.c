#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "attr.h"
#include "exc.h"
#include "str.h"
#include "object.h"
#include "util.h"
#include "strobject.h"

RhoValue rho_strobj_make(RhoStr value)
{
	RhoStrObject *s = rho_obj_alloc(&rho_str_class);
	s->freeable = value.freeable;
	value.freeable = 0;
	s->str = value;
	return rho_makeobj(s);
}

RhoValue rho_strobj_make_direct(const char *value, const size_t len)
{
	RhoStrObject *s = rho_obj_alloc(&rho_str_class);
	char *copy = rho_malloc(len + 1);
	memcpy(copy, value, len);
	copy[len] = '\0';
	s->str = RHO_STR_INIT(copy, len, 0);
	s->freeable = 1;
	return rho_makeobj(s);
}

static RhoValue strobj_eq(RhoValue *this, RhoValue *other)
{
	if (!rho_is_a(other, &rho_str_class)) {
		return rho_makefalse();
	}

	RhoStrObject *s1 = rho_objvalue(this);
	RhoStrObject *s2 = rho_objvalue(other);
	return rho_makebool(rho_str_eq(&s1->str, &s2->str));
}

static RhoValue strobj_cmp(RhoValue *this, RhoValue *other)
{
	if (!rho_is_a(other, &rho_str_class)) {
		return rho_makeut();
	}

	RhoStrObject *s1 = rho_objvalue(this);
	RhoStrObject *s2 = rho_objvalue(other);
	return rho_makeint(rho_str_cmp(&s1->str, &s2->str));
}

static RhoValue strobj_hash(RhoValue *this)
{
	RhoStrObject *s = rho_objvalue(this);
	return rho_makeint(rho_str_hash(&s->str));
}

static bool strobj_nonzero(RhoValue *this)
{
	RhoStrObject *s = rho_objvalue(this);
	return (s->str.len != 0);
}

static void strobj_free(RhoValue *this)
{
	RhoStrObject *s = rho_objvalue(this);
	if (s->freeable) {
		RHO_FREE(s->str.value);
	}

	s->base.class->super->del(this);
}

static RhoValue strobj_str(RhoValue *this)
{
	RhoStrObject *s = rho_objvalue(this);
	rho_retaino(s);
	return *this;
}

static RhoValue strobj_cat(RhoValue *this, RhoValue *other)
{
	if (!rho_is_a(other, &rho_str_class)) {
		return rho_makeut();
	}

	RhoStr *s1 = &((RhoStrObject *) rho_objvalue(this))->str;
	RhoStr *s2 = &((RhoStrObject *) rho_objvalue(other))->str;

	const size_t len1 = s1->len;
	const size_t len2 = s2->len;
	const size_t len_cat = len1 + len2;

	char *cat = rho_malloc(len_cat + 1);

	for (size_t i = 0; i < len1; i++) {
		cat[i] = s1->value[i];
	}

	for (size_t i = 0; i < len2; i++) {
		cat[i + len1] = s2->value[i];
	}

	cat[len_cat] = '\0';

	return rho_strobj_make(RHO_STR_INIT(cat, len_cat, 1));
}

static RhoValue strobj_len(RhoValue *this)
{
	RhoStrObject *s = rho_objvalue(this);
	return rho_makeint(s->str.len);
}

struct rho_num_methods rho_str_num_methods = {
	NULL,    /* plus */
	NULL,    /* minus */
	NULL,    /* abs */

	strobj_cat,    /* add */
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

	strobj_nonzero,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct rho_seq_methods rho_str_seq_methods = {
	strobj_len,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_str_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Str",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoStrObject),

	.init = NULL,
	.del = strobj_free,

	.eq = strobj_eq,
	.hash = strobj_hash,
	.cmp = strobj_cmp,
	.str = strobj_str,
	.call = NULL,

	.num_methods = &rho_str_num_methods,
	.seq_methods = &rho_str_seq_methods,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
