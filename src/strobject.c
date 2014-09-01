#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include "attr.h"
#include "err.h"
#include "str.h"
#include "object.h"
#include "strobject.h"

Value strobj_make(Str value)
{
	StrObject *s = malloc(sizeof(StrObject));
	s->base = (Object){.class = &str_class, .refcnt = 0};
	s->freeable = true;
	s->str = value;
	return makeobj(s);
}

static bool strobj_eq(Value *this, Value *other)
{
	type_assert(other, &str_class);
	StrObject *s1 = this->data.o;
	StrObject *s2 = this->data.o;
	return str_eq(&s1->str, &s2->str);
}

static int strobj_cmp(Value *this, Value *other)
{
	type_assert(other, &str_class);
	StrObject *s1 = this->data.o;
	StrObject *s2 = this->data.o;
	return str_cmp(&s1->str, &s2->str);
}

static int strobj_hash(Value *this)
{
	StrObject *s = this->data.o;
	return str_hash(&s->str);
}

static void strobj_free(Value *this)
{
	StrObject *s = this->data.o;
	if (s->freeable) {
		free((char *) s->str.value);
	}

	s->base.class->super->del(this);
}

static Str *strobj_str(Value *this)
{
	StrObject *s = this->data.o;
	return &s->str;
}

static Value strobj_cat(Value *this, Value *other)
{
	type_assert(other, &str_class);

	Str *s1 = &((StrObject *) this->data.o)->str;
	Str *s2 = &((StrObject *) other->data.o)->str;

	const size_t len1 = s1->len;
	const size_t len2 = s2->len;
	const size_t len_cat = len1 + len2;

	char *cat = malloc(len_cat + 1);

	for (size_t i = 0; i < len1; i++) {
		cat[i] = s1->value[i];
	}

	for (size_t i = 0; i < len2; i++) {
		cat[i + len1] = s2->value[i];
	}

	cat[len_cat] = '\0';

	return strobj_make((Str){.value = cat, .len = len_cat, .hashed = false});
}

struct num_methods str_num_methods = {
	NULL,    /* plus */
	NULL,    /* minus */
	NULL,    /* abs */

	strobj_cat,    /* add */
	NULL,    /* sub */
	NULL,    /* mul */
	NULL,    /* div */
	NULL,    /* mod */
	NULL,    /* pow */

	NULL,    /* not */
	NULL,    /* and */
	NULL,    /* or */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	NULL,    /* iadd */
	NULL,    /* isub */
	NULL,    /* imul */
	NULL,    /* idiv */
	NULL,    /* imod */
	NULL,    /* ipow */

	NULL,    /* iand */
	NULL,    /* ior */
	NULL,    /* ixor */
	NULL,    /* ishiftl */
	NULL,    /* ishiftr */

	NULL,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct seq_methods str_seq_methods = {
	NULL,    /* len */
	NULL,    /* conctat */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* iter */
	NULL,    /* iternext */
};

struct attr_member str_members[] = {
		{"len", ATTR_T_SIZE_T, offsetof(StrObject, str) + offsetof(Str, len), 0},
		{NULL, 0, 0, 0}
};

Class str_class = {
	.name = "Str",

	.super = &obj_class,

	.new = NULL,
	.del = strobj_free,

	.eq = strobj_eq,
	.hash = strobj_hash,
	.cmp = strobj_cmp,
	.str = strobj_str,
	.call = NULL,

	.num_methods = &str_num_methods,
	.seq_methods  = &str_seq_methods,

	.members = str_members,
	.methods = NULL
};
