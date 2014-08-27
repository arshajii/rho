#ifndef SYMTAB_H
#define SYMTAB_H

#include <stdlib.h>
#include <stdbool.h>
#include "ast.h"
#include "str.h"

typedef struct st_symbol {
	Str *key;
	unsigned int id;
	int hash;

	struct st_symbol *next;  /* used internally to form hash table buckets */

	/* variable flags */
	unsigned bound_here : 1;
	unsigned global_var : 1;
	unsigned free_var   : 1;
	unsigned func_param : 1;
	unsigned decl_const : 1;
} STSymbol;

typedef enum {
	MODULE,    /* top-level code */
	FUNCTION,  /* function body */
	CLASS      /* class body */
} STEContext;

typedef struct st_entry {
	const char *name;
	STEContext context;

	/* name-to-symbol hash table */
	STSymbol **table;
	size_t size;
	size_t capacity;
	size_t threshold;

	size_t n_locals;

	unsigned int next_id;  /* used internally for assigning IDs to locals */

	struct st_entry *parent;

	/* child vector */
	struct st_entry **children;
	size_t n_children;
	size_t children_capacity;

	size_t child_pos;  /* used internally for traversing symbol tables */
} STEntry;

typedef struct {
	const char *filename;

	STEntry *ste_module;
	STEntry *ste_current;
} SymTable;

SymTable *st_new(const char *filename);

void populate_symtable(SymTable *st, Program *program);

STSymbol *ste_get_symbol(STEntry *ste, Str *ident);

void st_free(SymTable *st);

#endif /* SYMTAB_H */
