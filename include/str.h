#ifndef STR_H
#define STR_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct {
	const char *value;  // read-only; this should NEVER be mutated
	size_t len;

	int hash;
	bool hashed;
} Str;

Str *str_new(const char *value, size_t len);
Str str_new_direct(const char *value, size_t len);
Str *str_new_copy(const char *value, size_t len);

bool str_eq(Str *s1, Str *s2);
int str_cmp(Str *s1, Str *s2);
int str_hash(Str *str);

Str *str_cat(Str *s1, Str *s2);

void str_free(Str *str);

#endif /* STR_H */
