#ifndef SYMTAB_H
#define SYMTAB_H

#include <stdlib.h>
#include <stdbool.h>
#include "str.h"

#define ST_CAPACITY 16
#define ST_LOADFACTOR 0.75f

typedef struct STEntry {
	Str *key;
	unsigned int value;
	int hash;
	struct STEntry *next;
} STEntry;

/*
 * Simple symbol table
 */
typedef struct {
	STEntry **table;
	size_t size;
	size_t capacity;

	float load_factor;
	size_t threshold;

	unsigned int next_id;
} SymTable;

SymTable *st_new(void);
unsigned int id_for_var(SymTable *st, Str *key);
void st_free(SymTable *st);

#endif /* SYMTAB_H */
