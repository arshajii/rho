#include <stdlib.h>
#include <string.h>
#include "object.h"
#include "util.h"
#include "strdict.h"

#define STRDICT_INIT_TABLE_SIZE  32
#define STRDICT_LOAD_FACTOR      0.75f

#define HASH(key)  (rho_util_hash_secondary(rho_str_hash((key))))

typedef RhoStrDictEntry Entry;

static Entry **make_empty_table(const size_t capacity);
static void strdict_resize(RhoStrDict *dict, const size_t new_capacity);

void rho_strdict_init(RhoStrDict *dict)
{
	dict->table = make_empty_table(STRDICT_INIT_TABLE_SIZE);
	dict->count = 0;
	dict->capacity = STRDICT_INIT_TABLE_SIZE;
	dict->threshold = (size_t)(STRDICT_INIT_TABLE_SIZE * STRDICT_LOAD_FACTOR);
}

static Entry *make_entry(RhoStr *key, const int hash, RhoValue *value)
{
	Entry *entry = rho_malloc(sizeof(Entry));
	entry->key = *key;
	entry->hash = hash;
	entry->value = *value;
	entry->next = NULL;
	return entry;
}

RhoValue rho_strdict_get(RhoStrDict *dict, RhoStr *key)
{
	const int hash = HASH(key);
	for (Entry *entry = dict->table[hash & (dict->capacity - 1)];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash && rho_str_eq(key, &entry->key)) {
			return entry->value;
		}
	}

	return rho_makeempty();
}

RhoValue rho_strdict_get_cstr(RhoStrDict *dict, const char *key)
{
	RhoStr key_str = RHO_STR_INIT(key, strlen(key), 0);
	return rho_strdict_get(dict, &key_str);
}

void rho_strdict_put(RhoStrDict *dict, const char *key, RhoValue *value, bool key_freeable)
{
	RhoStr key_str = RHO_STR_INIT(key, strlen(key), (key_freeable ? 1 : 0));
	Entry **table = dict->table;
	const size_t capacity = dict->capacity;
	const int hash = HASH(&key_str);
	const size_t index = hash & (capacity - 1);

	for (Entry *entry = table[index];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash && rho_str_eq(&key_str, &entry->key)) {
			rho_release(&entry->value);

			if (entry->key.freeable) {
				rho_str_dealloc(&entry->key);
			}

			entry->key = key_str;
			entry->value = *value;
			return;
		}
	}

	Entry *entry = make_entry(&key_str, hash, value);
	entry->next = table[index];
	table[index] = entry;

	if (dict->count++ >= dict->threshold) {
		const size_t new_capacity = 2 * capacity;
		strdict_resize(dict, new_capacity);
		dict->threshold = (size_t)(new_capacity * STRDICT_LOAD_FACTOR);
	}
}

void rho_strdict_put_copy(RhoStrDict *dict, const char *key, size_t len, RhoValue *value)
{
	if (len == 0) {
		len = strlen(key);
	}
	char *key_copy = rho_malloc(len + 1);
	strcpy(key_copy, key);
	rho_strdict_put(dict, key_copy, value, true);
}

void rho_strdict_dealloc(RhoStrDict *dict)
{
	Entry **table = dict->table;
	const size_t capacity = dict->capacity;

	for (size_t i = 0; i < capacity; i++) {
		Entry *entry = table[i];
		while (entry != NULL) {
			Entry *temp = entry;
			entry = entry->next;
			rho_release(&temp->value);
			if (temp->key.freeable) {
				rho_str_dealloc(&temp->key);
			}
			free(temp);
		}
	}

	free(table);
}

static Entry **make_empty_table(const size_t capacity)
{
	Entry **table = rho_malloc(capacity * sizeof(Entry *));
	for (size_t i = 0; i < capacity; i++) {
		table[i] = NULL;
	}
	return table;
}

static void strdict_resize(RhoStrDict *dict, const size_t new_capacity)
{
	const size_t old_capacity = dict->capacity;
	Entry **old_table = dict->table;
	Entry **new_table = make_empty_table(new_capacity);

	for (size_t i = 0; i < old_capacity; i++) {
		for (Entry *entry = old_table[i];
		     entry != NULL;
		     entry = entry->next) {

			const size_t index = entry->hash & (new_capacity - 1);
			entry->next = new_table[index];
			new_table[index] = entry;
		}
	}

	free(old_table);
	dict->table = new_table;
	dict->capacity = new_capacity;
}
