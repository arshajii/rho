#ifndef RHO_STR_H
#define RHO_STR_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct {
	const char *value;  // read-only; this should NEVER be mutated
	size_t len;

	int hash;
	unsigned hashed : 1;
	unsigned freeable : 1;
} RhoStr;

#define RHO_STR_INIT(v, l, f) ((RhoStr){.value = (v), .len = (l), .hash = 0, .hashed = 0, .freeable = (f)})

RhoStr *rho_str_new(const char *value, const size_t len);
RhoStr *rho_str_new_copy(const char *value, const size_t len);

bool rho_str_eq(RhoStr *s1, RhoStr *s2);
int rho_str_cmp(RhoStr *s1, RhoStr *s2);
int rho_str_hash(RhoStr *str);

RhoStr *rho_str_cat(RhoStr *s1, RhoStr *s2);

void rho_str_dealloc(RhoStr *str);
void rho_str_free(RhoStr *str);

#endif /* RHO_STR_H */
