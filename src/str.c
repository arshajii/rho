#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "str.h"

Str *str_new(const char *value, size_t len)
{
	Str *str = malloc(sizeof(Str));
	str->value = value;
	str->len = len;
	str->hash = 0;
	str->hashed = 0;
	str->freeable = 0;
	return str;
}

Str str_new_direct(const char *value, size_t len)
{
	Str str;
	str.value = value;
	str.len = len;
	str.hash = 0;
	str.hashed = 0;
	str.freeable = 0;
	return str;
}

Str *str_new_copy(const char *value, size_t len)
{
	Str *str = malloc(sizeof(Str));
	char *copy = malloc(len + 1);
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
	if (str->hashed) {
		return str->hash;
	}

	int h = str->hash;

	if (h == 0 && str->len > 0) {
		const size_t len = str->len;

		for (size_t i = 0; i < len; i++) {
			h = 31*h + str->value[i];
		}

		str->hash = h;
	}

	str->hashed = true;
	return h;
}

Str *str_cat(Str *s1, Str *s2)
{
	const size_t len1 = s1->len, len2 = s2->len;
	const size_t len_cat = len1 + len2;

	char *cat = malloc(len_cat + 1);

	for (size_t i = 0; i < len1; i++) {
		cat[i] = s1->value[i];
	}

	for (size_t i = 0; i < len2; i++) {
		cat[i + len1] = s2->value[i];
	}

	cat[len_cat] = '\0';

	return str_new(cat, len_cat);
}

void str_free(Str *str)
{
	free((char *)str->value);
	free(str);
}
