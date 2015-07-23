#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "str.h"

Str *str_new(const char *value, const size_t len)
{
	Str *str = rho_malloc(sizeof(Str));
	str->value = value;
	str->len = len;
	str->hash = 0;
	str->hashed = 0;
	str->freeable = 0;
	return str;
}

Str *str_new_copy(const char *value, const size_t len)
{
	Str *str = rho_malloc(sizeof(Str));
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

bool str_eq(Str *s1, Str *s2)
{
	if (s1->len != s2->len) {
		return false;
	}

	return memcmp(s1->value, s2->value, s1->len) == 0;
}

int str_cmp(Str *s1, Str *s2)
{
	return strcmp(s1->value, s2->value);
}

int str_hash(Str *str)
{
	if (!str->hashed) {
		str->hash = hash_cstr2(str->value, str->len);
		str->hashed = 1;
	}

	return str->hash;
}

Str *str_cat(Str *s1, Str *s2)
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

	return str_new(cat, len_cat);
}

void str_dealloc(Str *str)
{
	free((char *)str->value);
}

void str_free(Str *str)
{
	str_dealloc(str);
	free(str);
}
