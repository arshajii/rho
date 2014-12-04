#ifndef STR_H
#define STR_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct {
	const char *value;  // read-only; this should NEVER be mutated
	size_t len;

	int hash;
	unsigned hashed : 1;
	unsigned freeable : 1;
} Str;

#define STR_INIT(v, l, f) ((Str){.value = (v), .len = (l), .hash = 0, .hashed = 0, .freeable = (f)})

Str *str_new(const char *value, const size_t len);
Str *str_new_copy(const char *value, const size_t len);

bool str_eq(Str *s1, Str *s2);
int str_cmp(Str *s1, Str *s2);
int str_hash(Str *str);

Str *str_cat(Str *s1, Str *s2);

void str_free(Str *str);

#endif /* STR_H */
