#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "ast.h"
#include "str.h"
#include "util.h"
#include "err.h"
#include "symtab.h"

#define STE_INIT_CAPACITY             16
#define STE_LOADFACTOR                0.75f
#define STE_INIT_CHILDVEC_CAPACITY    5

#define FLAG_BOUND_HERE               (1 << 2)
#define FLAG_GLOBAL_VAR               (1 << 3)
#define FLAG_FREE_VAR                 (1 << 4)
#define FLAG_FUNC_PARAM               (1 << 5)
#define FLAG_DECL_CONST               (1 << 6)
#define FLAG_ATTRIBUTE                (1 << 7)

#define HASH(ident) (secondary_hash(str_hash((ident))))

static STEntry *ste_new(const char *name, STEContext context);

static void ste_grow(STEntry *ste, const size_t new_capacity);
static void ste_grow_attr(STEntry *ste, const size_t new_capacity);

static bool ste_register_ident(STEntry *ste, Str *ident, int flags);
static bool ste_register_attr(STEntry *ste, Str *ident);

static void populate_symtable_from_node(SymTable *st, AST *ast);
static void register_bindings(SymTable *st, Program *program);
static void register_bindings_from_node(SymTable *st, AST *ast);

static void ste_add_child(STEntry *ste, STEntry *child);
static void clear_child_pos(SymTable *st);
static void clear_child_pos_of_entry(STEntry *ste);

static void ste_free(STEntry *ste);

SymTable *st_new(const char *filename)
{
	SymTable *st = rho_malloc(sizeof(SymTable));
	st->filename = filename;

	STEntry *ste_module = ste_new("<module>", MODULE);
	STEntry *ste_attributes = ste_new("<attributes>", -1);

	ste_module->sym_table = ste_attributes->sym_table = st;

	st->ste_module = ste_module;
	st->ste_current = st->ste_module;
	st->ste_attributes = ste_attributes;

	return st;
}

static STEntry *ste_new(const char *name, STEContext context)
{
	// the capacity should always be a power of 2
	assert((STE_INIT_CAPACITY & (STE_INIT_CAPACITY - 1)) == 0);

	STEntry *ste = rho_malloc(sizeof(STEntry));
	ste->name = name;
	ste->context = context;
	ste->table = rho_malloc(STE_INIT_CAPACITY * sizeof(STSymbol *));
	for (size_t i = 0; i < STE_INIT_CAPACITY; i++) {
		ste->table[i] = NULL;
	}
	ste->table_size = 0;
	ste->table_capacity = STE_INIT_CAPACITY;
	ste->table_threshold = (size_t)(STE_INIT_CAPACITY * STE_LOADFACTOR);
	ste->next_local_id = 0;
	ste->n_locals = 0;
	ste->attributes = rho_malloc(STE_INIT_CAPACITY * sizeof(STSymbol *));
	for (size_t i = 0; i < STE_INIT_CAPACITY; i++) {
		ste->attributes[i] = NULL;
	}
	ste->attr_size = 0;
	ste->attr_capacity = STE_INIT_CAPACITY;
	ste->attr_threshold = (size_t)(STE_INIT_CAPACITY * STE_LOADFACTOR);
	ste->next_attr_id = 0;
	ste->next_free_var_id = 0;
	ste->parent = NULL;
	ste->children = rho_malloc(STE_INIT_CHILDVEC_CAPACITY * sizeof(STEntry));
	ste->n_children = 0;
	ste->children_capacity = STE_INIT_CHILDVEC_CAPACITY;
	ste->child_pos = 0;

	return ste;
}

void populate_symtable(SymTable *st, Program *program)
{
	register_bindings(st, program);
	for (struct ast_list *node = program; node != NULL; node = node->next) {
		populate_symtable_from_node(st, node->ast);
	}
	clear_child_pos(st);
}

static void populate_symtable_from_node(SymTable *st, AST *ast)
{
	if (ast == NULL) {
		return;
	}

	switch (ast->type) {
	case NODE_IDENT: {
		STEntry *module = st->ste_module;
		STEntry *current = st->ste_current;
		Str *ident = ast->v.ident;
		STSymbol *symbol = ste_get_symbol(current, ident);

		if (symbol == NULL) {
			/*
			 * Since `symbol` is null, we know the name in question is not
			 * bound in this scope, so we travel upwards through the symbol
			 * table hierarchy in order to find where the name is bound, be
			 * it in a lexically enclosing scope or in the global scope.
			 */
			if (ste_get_symbol(module, ident) != NULL) {
				ste_register_ident(current, ident, FLAG_GLOBAL_VAR);
			} else {
				ste_register_ident(current, ident, FLAG_FREE_VAR);
			}
		}

		break;
	}
	case NODE_DOT: {
		STEntry *current = st->ste_current;
		Str *attr = ast->right->v.ident;
		ste_register_attr(current, attr);
		populate_symtable_from_node(st, ast->left);
		break;
	}
	case NODE_IF:
		populate_symtable_from_node(st, ast->left);
		populate_symtable_from_node(st, ast->right);

		for (AST *node = ast->v.middle; node != NULL; node = node->v.middle) {
			populate_symtable_from_node(st, node);
		}
		break;
	case NODE_FOR:
	case NODE_COND_EXPR:
		populate_symtable_from_node(st, ast->left);
		populate_symtable_from_node(st, ast->right);
		populate_symtable_from_node(st, ast->v.middle);
		break;
	case NODE_ASSIGN:
		if (ast->left->type != NODE_IDENT) {
			populate_symtable_from_node(st, ast->left);
		}
		populate_symtable_from_node(st, ast->right);
		break;
	case NODE_BLOCK:
		for (struct ast_list *node = ast->v.block; node != NULL; node = node->next) {
			populate_symtable_from_node(st, node->ast);
		}
		break;
	case NODE_LIST:
	case NODE_TUPLE:
		for (struct ast_list *node = ast->v.list; node != NULL; node = node->next) {
			populate_symtable_from_node(st, node->ast);
		}
		break;
	case NODE_DEF: {
		assert(ast->left->type == NODE_IDENT);
		assert(ast->right->type == NODE_BLOCK);

		for (struct ast_list *node = ast->v.params; node != NULL; node = node->next) {
			if (node->ast->type == NODE_ASSIGN) {
				populate_symtable_from_node(st, node->ast->right);
			}
		}

		STEntry *parent = st->ste_current;
		STEntry *child = parent->children[parent->child_pos++];

		st->ste_current = child;
		populate_symtable_from_node(st, ast->right);
		st->ste_current = parent;
		break;
	}
	case NODE_LAMBDA: {
		STEntry *parent = st->ste_current;
		STEntry *child = parent->children[parent->child_pos++];

		st->ste_current = child;
		populate_symtable_from_node(st, ast->left);
		st->ste_current = parent;
		break;
	}
	case NODE_CALL: {
		populate_symtable_from_node(st, ast->left);
		for (struct ast_list *node = ast->v.params; node != NULL; node = node->next) {
			populate_symtable_from_node(st, node->ast);
		}
		break;
	}
	case NODE_TRY_CATCH: {
		populate_symtable_from_node(st, ast->left);
		populate_symtable_from_node(st, ast->right);
		for (struct ast_list *node = ast->v.excs; node != NULL; node = node->next) {
			populate_symtable_from_node(st, node->ast);
		}
		break;
	}
	default:
		populate_symtable_from_node(st, ast->left);
		populate_symtable_from_node(st, ast->right);
		break;
	}
}

/*
 * Creates the tree structure of the symbol table, but only adds
 * binding data (i.e. which variables are bound in which scope).
 */
static void register_bindings(SymTable *st, Program *program)
{
	for (struct ast_list *node = program; node != NULL; node = node->next) {
		register_bindings_from_node(st, node->ast);
	}
}

static void register_bindings_from_node(SymTable *st, AST *ast)
{
	if (ast == NULL) {
		return;
	}

	const bool global = (st->ste_current == st->ste_module);

	switch (ast->type) {
	case NODE_ASSIGN:
		if (ast->left->type == NODE_IDENT) {
			int flag = FLAG_BOUND_HERE;
			if (global) {
				flag |= FLAG_GLOBAL_VAR;
			}
			ste_register_ident(st->ste_current, ast->left->v.ident, flag);
			register_bindings_from_node(st, ast->right);
		}
		break;
	case NODE_FOR:
		ste_register_ident(st->ste_current, ast->left->v.ident, FLAG_BOUND_HERE);
		register_bindings_from_node(st, ast->right);
		register_bindings_from_node(st, ast->v.middle);
		break;
	case NODE_IMPORT: {
		int flag = FLAG_BOUND_HERE;
		if (global) {
			flag |= FLAG_GLOBAL_VAR;
		}
		ste_register_ident(st->ste_current, ast->left->v.ident, flag);
		break;
	}
	case NODE_IF:
	case NODE_ELIF:
		register_bindings_from_node(st, ast->left);
		register_bindings_from_node(st, ast->right);
		register_bindings_from_node(st, ast->v.middle);
		break;
	case NODE_BLOCK:
		for (struct ast_list *node = ast->v.block; node != NULL; node = node->next) {
			register_bindings_from_node(st, node->ast);
		}
		break;
	case NODE_DEF: {
		assert(ast->left->type == NODE_IDENT);
		assert(ast->right->type == NODE_BLOCK);

		Str *name = ast->left->v.ident;
		ste_register_ident(st->ste_current,
		                   name,
		                   (global ? FLAG_GLOBAL_VAR : 0) | FLAG_BOUND_HERE);

		STEntry *child = ste_new(name->value, FUNCTION);

		for (struct ast_list *param = ast->v.params; param != NULL; param = param->next) {
			Str *ident = (param->ast->type == NODE_ASSIGN) ? param->ast->left->v.ident :
			                                                 param->ast->v.ident;

			const bool param_seen = ste_register_ident(child, ident, FLAG_BOUND_HERE | FLAG_FUNC_PARAM);
			assert(!param_seen);
		}

		ste_add_child(st->ste_current, child);
		st->ste_current = child;
		register_bindings_from_node(st, ast->right);
		st->ste_current = child->parent;
		break;
	}
	case NODE_LAMBDA: {
		STEntry *child = ste_new("<lambda>", FUNCTION);
		const unsigned int max_dollar_ident = ast->v.max_dollar_ident;
		assert(max_dollar_ident <= 128);

		char buf[4];
		for (unsigned i = 1; i <= max_dollar_ident; i++) {
			sprintf(buf, "$%u", i);
			Str *ident = str_new_copy(buf, strlen(buf));
			ident->freeable = 1;
			ste_register_ident(child, ident, FLAG_BOUND_HERE | FLAG_FUNC_PARAM);
		}

		ste_add_child(st->ste_current, child);
		st->ste_current = child;
		register_bindings_from_node(st, ast->left);
		st->ste_current = child->parent;
		break;
	}
	case NODE_CALL: {
		register_bindings_from_node(st, ast->left);
		for (struct ast_list *node = ast->v.params; node != NULL; node = node->next) {
			register_bindings_from_node(st, node->ast);
		}
		break;
	}
	case NODE_LIST:
	case NODE_TUPLE:
		for (struct ast_list *node = ast->v.list; node != NULL; node = node->next) {
			register_bindings_from_node(st, node->ast);
		}
		break;
	default:
		register_bindings_from_node(st, ast->left);
		register_bindings_from_node(st, ast->right);
		break;
	}
}

/*
 * XXX: There's a lot of code duplication here.
 */

STSymbol *ste_get_symbol(STEntry *ste, Str *ident)
{
	const int hash = HASH(ident);
	const size_t index = hash & (ste->table_capacity - 1);

	for (STSymbol *sym = ste->table[index]; sym != NULL; sym = sym->next) {
		if (hash == sym->hash && str_eq(ident, sym->key)) {
			return sym;
		}
	}

	return NULL;
}

STSymbol *ste_get_attr_symbol(STEntry *ste, Str *attr)
{
	const int hash = HASH(attr);
	const size_t index = hash & (ste->attr_capacity - 1);

	for (STSymbol *sym = ste->attributes[index]; sym != NULL; sym = sym->next) {
		if (hash == sym->hash && str_eq(attr, sym->key)) {
			return sym;
		}
	}

	return NULL;
}

/*
 * Registers the given identifier; returns whether the identifier
 * had been previously registered.
 */
static bool ste_register_ident(STEntry *ste, Str *ident, int flags)
{
	STSymbol *symbol = ste_get_symbol(ste, ident);
	bool found = true;

	if (symbol == NULL) {
		found = false;
		const int hash = HASH(ident);
		const size_t index = hash & (ste->table_capacity - 1);

		symbol = rho_calloc(1, sizeof(STSymbol));
		symbol->key = ident;

		if (flags & FLAG_BOUND_HERE) {
			symbol->id = ste->next_local_id++;
		} else if (flags & FLAG_GLOBAL_VAR) {
			STEntry *module = ste;

			while (module->parent != NULL) {
				module = module->parent;
			}

			STSymbol *global_symbol = ste_get_symbol(module, ident);

			if (global_symbol == NULL) {
				INTERNAL_ERROR();
			}

			symbol->id = global_symbol->id;
		} else {
			assert(flags & FLAG_FREE_VAR);
			symbol->id = ste->next_free_var_id++;
		}

		symbol->hash = HASH(ident);
		symbol->next = ste->table[index];

		ste->table[index] = symbol;

		++ste->table_size;

		if (ste->table_size > ste->table_threshold) {
			ste_grow(ste, 2 * ste->table_capacity);
		}
	}

	if (flags & FLAG_BOUND_HERE) {
		symbol->bound_here = 1;
	}

	if (flags & FLAG_GLOBAL_VAR) {
		symbol->global_var = 1;
	}

	if (flags & FLAG_FREE_VAR) {
		symbol->free_var = 1;
	}

	if (flags & FLAG_FUNC_PARAM) {
		symbol->func_param = 1;
	}

	if (flags & FLAG_DECL_CONST) {
		symbol->decl_const = 1;
	}

	/* this should be handled separately */
	if (flags & FLAG_ATTRIBUTE) {
		INTERNAL_ERROR();
	}

	return found;
}

/*
 * Registers the given attribute; returns whether the attribute
 * had been previously registered.
 */
static bool ste_register_attr(STEntry *ste, Str *attr)
{
	STSymbol *symbol = ste_get_attr_symbol(ste, attr);
	bool found = true;

	if (symbol == NULL) {
		found = false;
		const int hash = HASH(attr);
		const size_t index = hash & (ste->attr_capacity - 1);

		symbol = rho_calloc(1, sizeof(STSymbol));
		symbol->key = attr;
		symbol->attribute = 1;
		symbol->id = ste->next_attr_id++;
		symbol->hash = HASH(attr);
		symbol->next = ste->attributes[index];
		ste->attributes[index] = symbol;

		if (ste->next_attr_id > ste->attr_threshold) {
			ste_grow_attr(ste, 2 * ste->attr_capacity);
		}
	}

	return found;
}

static void ste_grow(STEntry *ste, const size_t new_capacity)
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

	STSymbol **new_table = rho_malloc(capacity_real * sizeof(STSymbol *));
	for (size_t i = 0; i < capacity_real; i++) {
		new_table[i] = NULL;
	}

	const size_t capacity = ste->table_capacity;

	for (size_t i = 0; i < capacity; i++) {
		STSymbol *e = ste->table[i];

		while (e != NULL) {
			STSymbol *next = e->next;
			const size_t index = (e->hash & (capacity_real - 1));
			e->next = new_table[index];
			new_table[index] = e;
			e = next;
		}
	}

	free(ste->table);
	ste->table = new_table;
	ste->table_capacity = capacity_real;
	ste->table_threshold = (size_t)(capacity_real * STE_LOADFACTOR);
}

static void ste_grow_attr(STEntry *ste, const size_t new_capacity)
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

	STSymbol **new_attributes = rho_malloc(capacity_real * sizeof(STSymbol *));
	for (size_t i = 0; i < capacity_real; i++) {
		new_attributes[i] = NULL;
	}

	const size_t capacity = ste->attr_capacity;

	for (size_t i = 0; i < capacity; i++) {
		STSymbol *e = ste->attributes[i];

		while (e != NULL) {
			STSymbol *next = e->next;
			const size_t index = (e->hash & (capacity_real - 1));
			e->next = new_attributes[index];
			new_attributes[index] = e;
			e = next;
		}
	}

	free(ste->attributes);
	ste->attributes = new_attributes;
	ste->attr_capacity = capacity_real;
	ste->attr_threshold = (size_t)(capacity_real * STE_LOADFACTOR);
}

static void ste_add_child(STEntry *ste, STEntry *child)
{
	child->sym_table = ste->sym_table;

	if (ste->n_children == ste->children_capacity) {
		ste->children_capacity = (ste->children_capacity * 3)/2 + 1;
		ste->children = rho_realloc(ste->children, ste->children_capacity);
	}

	ste->children[ste->n_children++] = child;
	child->parent = ste;
}

static void clear_child_pos(SymTable *st)
{
	clear_child_pos_of_entry(st->ste_module);
}

static void clear_child_pos_of_entry(STEntry *ste)
{
	ste->child_pos = 0;

	const size_t n_children = ste->n_children;

	for (size_t i = 0; i < n_children; i++) {
		clear_child_pos_of_entry(ste->children[i]);
	}
}

void st_free(SymTable *st)
{
	ste_free(st->ste_module);
	ste_free(st->ste_attributes);
	free(st);
}

static void stsymbol_free(STSymbol *entry)
{
	while (entry != NULL) {
		STSymbol *temp = entry;
		entry = entry->next;

		if (temp->key->freeable) {
			str_free(temp->key);
		}

		free(temp);
	}
}

static void ste_free(STEntry *ste)
{
	const size_t table_capacity = ste->table_capacity;
	const size_t attr_capacity = ste->attr_capacity;

	for (size_t i = 0; i < table_capacity; i++) {
		stsymbol_free(ste->table[i]);
	}

	free(ste->table);

	for (size_t i = 0; i < attr_capacity; i++) {
		stsymbol_free(ste->attributes[i]);
	}

	free(ste->attributes);

	STEntry **children = ste->children;
	size_t n_children = ste->n_children;
	for (size_t i = 0; i < n_children; i++) {
		ste_free(children[i]);
	}

	free(ste->children);
	free(ste);
}
