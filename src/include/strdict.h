#ifndef RHO_STRDICT_H
#define RHO_STRDICT_H

#include <stdlib.h>
#include "str.h"
#include "object.h"

typedef struct str_dict_entry {
	RhoStr key;
	int hash;
	RhoValue value;
	struct str_dict_entry *next;
} RhoStrDictEntry;

typedef struct {
	RhoStrDictEntry **table;
	size_t count;
	size_t capacity;
	size_t threshold;
} RhoStrDict;

void rho_strdict_init(RhoStrDict *dict);
RhoValue rho_strdict_get(RhoStrDict *dict, RhoStr *key);
RhoValue rho_strdict_get_cstr(RhoStrDict *dict, const char *key);
void rho_strdict_put(RhoStrDict *dict, const char *key, RhoValue *value, bool key_freeable);
void rho_strdict_put_copy(RhoStrDict *dict, const char *key, size_t len, RhoValue *value);
void rho_strdict_dealloc(RhoStrDict *dict);

#endif /* RHO_STRDICT_H */
