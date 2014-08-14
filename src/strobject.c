#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "err.h"
#include "str.h"
#include "strobject.h"

Value strobj_make(Str value)
{
	StrObject *s = malloc(sizeof(StrObject));
	s->base = (Object){.class = &str_class, .refcnt = 0};
	s->freeable = true;
	s->str = value;
	return (Value){.type = VAL_TYPE_OBJECT, .data = {.o = s}};
}

static Value strobj_eq(Value *this, Value *other)
{
	type_assert(other, &str_class);
	StrObject *s1 = this->data.o;
	StrObject *s2 = this->data.o;
	return (Value){.type = VAL_TYPE_INT, .data = {.i = str_eq(&s1->str, &s2->str)}};
}

static Value strobj_cmp(Value *this, Value *other)
{
	type_assert(other, &str_class);
	StrObject *s1 = this->data.o;
	StrObject *s2 = this->data.o;
	return (Value){.type = VAL_TYPE_INT, .data = {.i = str_cmp(&s1->str, &s2->str)}};
}

static Value strobj_hash(Value *this)
{
	StrObject *s = this->data.o;
	return (Value){.type = VAL_TYPE_INT, .data = {.i = str_hash(&s->str)}};
}

static void strobj_free(Value *this)
{
	StrObject *s = this->data.o;
	if (s->freeable) {
		free((char *) s->str.value);
	}

	s->base.class->super->free(this);
}

static Value strobj_str(Value *this)
{
	return *this;
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

struct arith_methods str_arith_methods = {
	NULL,			/*  uplus   */
	NULL,			/*  uminus  */
	strobj_cat,		/*  add     */
	NULL,			/*  sub     */
	NULL,			/*  mul     */
	NULL,			/*  div     */
	NULL,			/*  mod     */
	NULL			/*  pow     */
};

struct cmp_methods str_cmp_methods = {
	strobj_eq,		/*  eq    */
	strobj_cmp,		/*  cmp   */
	strobj_hash,	/*  hash  */
};

struct misc_methods str_misc_methods = {
	NULL,			/*  call     */
	NULL,			/*  index    */
	NULL,			/*  nonzero  */
};

Class str_class = {
	.name = "Str",

	.super = &obj_class,

	.new = NULL,
	.free = strobj_free,
	.str = strobj_str,

	.arith_methods = &str_arith_methods,
	.cmp_methods = &str_cmp_methods,
	.misc_methods = &str_misc_methods
};
