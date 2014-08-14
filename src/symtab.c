#include <stdlib.h>
#include <stdbool.h>
#include "str.h"
#include "util.h"
#include "symtab.h"

static SymTable *st_new_specific(const size_t capacity, const float load_factor);
static void st_grow(SymTable *st, const size_t new_capacity);

SymTable *st_new(void)
{
	return st_new_specific(ST_CAPACITY, ST_LOADFACTOR);
}

static SymTable *st_new_specific(const size_t capacity, const float load_factor)
{
	// the capacity should be a power of 2:
	size_t capacity_real;

	if ((capacity & (capacity - 1)) == 0) {
		capacity_real = capacity;
	} else {
		capacity_real = 1;
		while (capacity_real < capacity)
			capacity_real <<= 1;
	}

	SymTable *st = malloc(sizeof(SymTable));
	st->table = calloc(capacity_real, sizeof(STEntry *));
	st->size = 0;
	st->capacity = capacity_real;

	st->load_factor = load_factor;
	st->threshold = (size_t)(capacity * load_factor);

	st->next_id = 0;

	return st;
}

unsigned int id_for_var(SymTable *st, Str *key)
{
	const int hash = secondary_hash(str_hash(key));
	const size_t index = hash & (st->capacity - 1);

	for (STEntry *entry = st->table[index]; entry != NULL; entry = entry->next) {
		if (hash == entry->hash && str_eq(key, entry->key)) {
			return entry->value;
		}
	}

	STEntry *new = malloc(sizeof(STEntry));
	new->key = key;
	new->value = st->next_id++;
	new->hash = hash;
	new->next = st->table[index];

	st->table[index] = new;

	++st->size;

	if (st->size > st->threshold) {
		st_grow(st, 2 * st->capacity);
	}

	return new->value;
}

void st_grow(SymTable *st, const size_t new_capacity)
{
	if (new_capacity == 0) {
		return;
	}

	// the capacity should be a power of 2:
	size_t capacity_real;

	if ((new_capacity & (new_capacity - 1)) == 0) {
		capacity_real = new_capacity;
	} else {
		capacity_real = 1;

		while (capacity_real < new_capacity) {
			capacity_real <<= 1;
		}
	}

	STEntry **new_table = calloc(capacity_real, sizeof(STEntry *));

	const size_t capacity = st->capacity;

	for (size_t i = 0; i < capacity; i++) {
		STEntry *e = st->table[i];

		while (e != NULL) {
			STEntry *next = e->next;
			const size_t index = (e->hash & (capacity_real - 1));
			e->next = new_table[index];
			new_table[index] = e;
			e = next;
		}
	}

	free(st->table);
	st->table = new_table;
	st->capacity = capacity_real;
	st->threshold = (size_t)(capacity_real * st->load_factor);
}

void st_free(SymTable *st)
{
	const size_t capacity = st->capacity;

	for (size_t i = 0; i < capacity; i++) {
		STEntry *entry = st->table[i];

		while (entry != NULL) {
			STEntry *temp = entry;
			entry = entry->next;
			free(temp);
		}
	}

	free(st->table);
	free(st);
}
