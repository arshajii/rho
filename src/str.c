#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "str.h"

RhoStr *rho_str_new(const char *value, const size_t len)
{
	RhoStr *str = rho_malloc(sizeof(RhoStr));
	str->value = value;
	str->len = len;
	str->hash = 0;
	str->hashed = 0;
	str->freeable = 0;
	return str;
}

RhoStr *rho_str_new_copy(const char *value, const size_t len)
{
	RhoStr *str = rho_malloc(sizeof(RhoStr));
	char *copy = rho_malloc(len + 1);
	memcpy(copy, value, len);
	copy[len] = '\0';
	str->value = copy;

	str->len = len;
	str->hash = 0;
	str->hashed = 0;
	str->freeable = 0;

	return str;
}

bool rho_str_eq(RhoStr *s1, RhoStr *s2)
{
	if (s1->len != s2->len) {
		return false;
	}

	return memcmp(s1->value, s2->value, s1->len) == 0;
}

int rho_str_cmp(RhoStr *s1, RhoStr *s2)
{
	return strcmp(s1->value, s2->value);
}

int rho_str_hash(RhoStr *str)
{
	if (!str->hashed) {
		str->hash = rho_util_hash_cstr2(str->value, str->len);
		str->hashed = 1;
	}

	return str->hash;
}

RhoStr *rho_str_cat(RhoStr *s1, RhoStr *s2)
{
	const size_t len1 = s1->len, len2 = s2->len;
	const size_t len_cat = len1 + len2;

	char *cat = rho_malloc(len_cat + 1);

	for (size_t i = 0; i < len1; i++) {
		cat[i] = s1->value[i];
	}

	for (size_t i = 0; i < len2; i++) {
		cat[i + len1] = s2->value[i];
	}

	cat[len_cat] = '\0';

	return rho_str_new(cat, len_cat);
}

void rho_str_dealloc(RhoStr *str)
{
	RHO_FREE(str->value);
}

void rho_str_free(RhoStr *str)
{
	rho_str_dealloc(str);
	free(str);
}

void rho_util_str_array_dup(struct rho_str_array *src, struct rho_str_array *dst)
{
	const size_t length = src->length;
	const size_t size = length * sizeof(*(src->array));
	dst->array = rho_malloc(size);
	dst->length = length;
	memcpy(dst->array, src->array, size);
}
