#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "attr.h"

static void attr_dict_put(AttrDict *d, const char *key, unsigned int attr_index, bool is_method);
static int hash(const char *key);

void attr_dict_init(AttrDict *d, const size_t max_size)
{
	const size_t capacity = max_size * 8/5;
	d->table = malloc(capacity * sizeof(AttrDictEntry *));
	for (size_t i = 0; i < capacity; i++) {
		d->table[i] = NULL;
	}
	d->table_capacity = capacity;
	d->table_size = 0;
}

unsigned int attr_dict_get(AttrDict *d, const char *key)
{
	const int h = hash(key);
	const size_t index = h & (d->table_capacity - 1);

	for (AttrDictEntry *e = d->table[index]; e != NULL; e = e->next) {
		if (h == e->hash && strcmp(key, e->key) == 0) {
			return e->value;
		}
	}

	return 0;
}

static void attr_dict_put(AttrDict *d, const char *key, unsigned int attr_index, bool is_method)
{
	unsigned int value = (attr_index << 2) | ATTR_DICT_FLAG_FOUND;
	if (is_method) {
		value |= ATTR_DICT_FLAG_METHOD;
	}

	const int h = hash(key);
	const size_t index = h & (d->table_capacity - 1);

	AttrDictEntry *e = malloc(sizeof(AttrDictEntry));
	e->key = key;
	e->value = value;
	e->hash = h;
	e->next = d->table[index];
	d->table[index] = e;
}

void attr_dict_register_members(AttrDict *d, struct attr_member *members)
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

void attr_dict_register_methods(AttrDict *d, struct attr_method *methods)
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

	int h = 0;
	while (*p) {
		h = 31*h + *p;
		++p;
	}

	return secondary_hash(h);
}
