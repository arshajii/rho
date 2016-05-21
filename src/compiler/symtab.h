#ifndef RHO_SYMTAB_H
#define RHO_SYMTAB_H

#include <stdlib.h>
#include "ast.h"
#include "str.h"

typedef struct rho_st_symbol {
	RhoStr *key;
	unsigned int id;
	int hash;

	struct rho_st_symbol *next;  /* used internally to form hash table buckets */

	/* variable flags */
	unsigned bound_here : 1;
	unsigned global_var : 1;
	unsigned free_var   : 1;
	unsigned func_param : 1;
	unsigned decl_const : 1;
	unsigned attribute  : 1;
} RhoSTSymbol;

typedef enum {
	RHO_MODULE,    /* top-level code */
	RHO_FUNCTION,  /* function body */
	RHO_CLASS      /* class body */
} RhoSTEContext;

struct rho_rho_sym_table;

typedef struct rho_st_entry {
	const char *name;
	RhoSTEContext context;

	/* name-to-symbol hash table */
	RhoSTSymbol **table;
	size_t table_size;
	size_t table_capacity;
	size_t table_threshold;

	unsigned int next_local_id;  /* used internally for assigning IDs to locals */
	size_t n_locals;

	/* attribute-to-symbol hash table */
	RhoSTSymbol **attributes;
	size_t attr_size;
	size_t attr_capacity;
	size_t attr_threshold;

	/* used internally for assigning IDs to attributes */
	unsigned int next_attr_id;

	/* used internally for assigning IDs to free variables */
	unsigned int next_free_var_id;

	struct rho_st_entry *parent;
	struct rho_rho_sym_table *sym_table;

	/* child vector */
	struct rho_st_entry **children;
	size_t n_children;
	size_t children_capacity;

	size_t child_pos;  /* used internally for traversing symbol tables */
} RhoSTEntry;

typedef struct rho_rho_sym_table {
	const char *filename;

	RhoSTEntry *ste_module;
	RhoSTEntry *ste_current;

	RhoSTEntry *ste_attributes;
} RhoSymTable;

RhoSymTable *rho_st_new(const char *filename);

void rho_st_populate(RhoSymTable *st, RhoProgram *program);

RhoSTSymbol *rho_ste_get_symbol(RhoSTEntry *ste, RhoStr *ident);

RhoSTSymbol *rho_ste_get_attr_symbol(RhoSTEntry *ste, RhoStr *ident);

void rho_st_free(RhoSymTable *st);

#endif /* RHO_SYMTAB_H */
