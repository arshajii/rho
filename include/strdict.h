#ifndef STRDICT_H
#define STRDICT_H

#include <stdlib.h>
#include "str.h"
#include "object.h"

typedef struct str_dict_entry {
	Str key;
	int hash;
	Value value;
	struct str_dict_entry *next;
} StrDictEntry;

typedef struct {
	StrDictEntry **table;
	size_t count;
	size_t capacity;
	size_t threshold;
} StrDict;

void strdict_init(StrDict *dict);
Value strdict_get(StrDict *dict, Str *key);
Value strdict_get_cstr(StrDict *dict, const char *key);
void strdict_put(StrDict *dict, const char *key, Value *value, bool key_freeable);
void strdict_put_copy(StrDict *dict, const char *key, size_t len, Value *value);
void strdict_apply_to_all(StrDict *dict, void (*fn)(Value *v, void *args), void *args);
void strdict_dealloc(StrDict *dict);

#endif /* STRDICT_H */
