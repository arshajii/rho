#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "err.h"
#include "attr.h"

static void attr_dict_put(RhoAttrDict *d, const char *key, unsigned int attr_index, bool is_method);
static int hash(const char *key);

void rho_attr_dict_init(RhoAttrDict *d, const size_t max_size)
{
	const size_t capacity = (max_size * 8)/5;
	d->table = rho_malloc(capacity * sizeof(RhoAttrDictEntry *));
	for (size_t i = 0; i < capacity; i++) {
		d->table[i] = NULL;
	}
	d->table_capacity = capacity;
	d->table_size = 0;
}

unsigned int rho_attr_dict_get(RhoAttrDict *d, const char *key)
{
	const size_t table_capacity = d->table_capacity;

	if (table_capacity == 0) {
		return 0;
	}

	const int h = hash(key);
	const size_t index = h & (table_capacity - 1);

	for (RhoAttrDictEntry *e = d->table[index]; e != NULL; e = e->next) {
		if (h == e->hash && strcmp(key, e->key) == 0) {
			return e->value;
		}
	}

	return 0;
}

static void attr_dict_put(RhoAttrDict *d, const char *key, unsigned int attr_index, bool is_method)
{
	const size_t table_capacity = d->table_capacity;

	if (table_capacity == 0) {
		RHO_INTERNAL_ERROR();
	}

	unsigned int value = (attr_index << 2) | RHO_ATTR_DICT_FLAG_FOUND;
	if (is_method) {
		value |= RHO_ATTR_DICT_FLAG_METHOD;
	}

	const int h = hash(key);
	const size_t index = h & (table_capacity - 1);

	RhoAttrDictEntry *e = rho_malloc(sizeof(RhoAttrDictEntry));
	e->key = key;
	e->value = value;
	e->hash = h;
	e->next = d->table[index];
	d->table[index] = e;
}

void rho_attr_dict_register_members(RhoAttrDict *d, struct rho_attr_member *members)
{
	if (members == NULL) {
		return;
	}

	unsigned int index = 0;
	while (members[index].name != NULL) {
		attr_dict_put(d, members[index].name, index, false);
		++index;
	}
}

void rho_attr_dict_register_methods(RhoAttrDict *d, struct rho_attr_method *methods)
{
	if (methods == NULL) {
		return;
	}

	unsigned int index = 0;
	while (methods[index].name != NULL) {
		attr_dict_put(d, methods[index].name, index, true);
		++index;
	}
}

/*
 * Hashes null-terminated string
 */
static int hash(const char *key)
{
	char *p = (char *)key;

	unsigned int h = 0;
	while (*p) {
		h = 31*h + *p;
		++p;
	}

	return rho_util_hash_secondary((int)h);
}
