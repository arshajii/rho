#include <stdlib.h>
#include <stdbool.h>
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

#define HASH(ident) (secondary_hash(str_hash((ident))))

static STEntry *ste_new(const char *name, STEContext context);

static void ste_grow(STEntry *ste, const size_t new_capacity);
static void ste_add_child(STEntry *ste, STEntry *child);
static void ste_register_ident(STEntry *ste, Str *ident, int flags);

static void populate_symtable_from_node(SymTable *st, AST *ast);
static void register_bindings(SymTable *st, Program *program);
static void register_bindings_from_node(SymTable *st, AST *ast);

static void clear_child_pos(SymTable *st);
static void clear_child_pos_of_entry(STEntry *ste);

static void ste_free(STEntry *ste);

SymTable *st_new(const char *filename)
{
	SymTable *st = malloc(sizeof(SymTable));
	st->filename = filename;
	st->ste_module = ste_new("<module>", MODULE);
	st->ste_current = st->ste_module;

	return st;
}

static STEntry *ste_new(const char *name, STEContext context)
{
	// the capacity should always be a power of 2
	assert((STE_INIT_CAPACITY & (STE_INIT_CAPACITY - 1)) == 0);

	STEntry *ste = malloc(sizeof(STEntry));
	ste->name = name;
	ste->context = context;
	ste->table = calloc(STE_INIT_CAPACITY, sizeof(STSymbol *));
	ste->size = 0;
	ste->capacity = STE_INIT_CAPACITY;
	ste->threshold = (size_t)(STE_INIT_CAPACITY * STE_LOADFACTOR);
	ste->n_locals = 0;
	ste->next_id = 0;
	ste->parent = NULL;
	ste->children = malloc(STE_INIT_CHILDVEC_CAPACITY * sizeof(STEntry));
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
	case NODE_IF:
		populate_symtable_from_node(st, ast->left);
		populate_symtable_from_node(st, ast->right);
		populate_symtable_from_node(st, ast->v.middle);
		break;
	case NODE_ASSIGN:
		if (ast->left->type == NODE_IDENT) {
			populate_symtable_from_node(st, ast->right);
		} else {
			populate_symtable_from_node(st, ast->left);
			populate_symtable_from_node(st, ast->right);
		}
		break;
	case NODE_BLOCK:
		for (struct ast_list *node = ast->v.block; node != NULL; node = node->next) {
			populate_symtable_from_node(st, node->ast);
		}
		break;
	case NODE_DEF: {
		assert(ast->left->type == NODE_IDENT);
		assert(ast->right->type == NODE_BLOCK);

		STEntry *parent = st->ste_current;
		STEntry *child = parent->children[parent->child_pos++];

		st->ste_current = child;
		populate_symtable_from_node(st, ast->right);
		st->ste_current = parent;
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
	case NODE_IF:
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

		ParamList *params = ast->v.params;
		while (params != NULL) {
			ste_register_ident(child, params->ast->v.ident, FLAG_BOUND_HERE | FLAG_FUNC_PARAM);
			params = params->next;
		}

		ste_add_child(st->ste_current, child);
		st->ste_current = child;
		register_bindings_from_node(st, ast->right);
		st->ste_current = child->parent;
		break;
	}
	default:
		register_bindings_from_node(st, ast->left);
		register_bindings_from_node(st, ast->right);
		break;
	}
}

STSymbol *ste_get_symbol(STEntry *ste, Str *ident)
{
	const int hash = HASH(ident);
	const size_t index = hash & (ste->capacity - 1);

	for (STSymbol *sym = ste->table[index]; sym != NULL; sym = sym->next) {
		if (hash == sym->hash && str_eq(ident, sym->key)) {
			return sym;
		}
	}

	return NULL;
}

static void ste_register_ident(STEntry *ste, Str *ident, int flags)
{
	STSymbol *symbol = ste_get_symbol(ste, ident);

	if (symbol == NULL) {
		const int hash = HASH(ident);
		const size_t index = hash & (ste->capacity - 1);

		symbol = malloc(sizeof(STSymbol));
		symbol->key = ident;

		if (flags & FLAG_BOUND_HERE) {
			symbol->id = ste->next_id++;
			++ste->n_locals;
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
			symbol->id = 0;
		}

		symbol->hash = HASH(ident);
		symbol->next = ste->table[index];

		ste->table[index] = symbol;

		++ste->size;

		if (ste->size > ste->threshold) {
			ste_grow(ste, 2 * ste->capacity);
		}
	}

	if (flags & FLAG_BOUND_HERE) {
		symbol->bound_here = 1;
	}

	if (flags & FLAG_DECL_CONST) {
		symbol->decl_const = 1;
	}

	if (flags & FLAG_GLOBAL_VAR) {
		symbol->global_var = 1;
	}

	if (flags & FLAG_FUNC_PARAM) {
		symbol->func_param = 1;
	}
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

	STSymbol **new_table = calloc(capacity_real, sizeof(STSymbol *));

	const size_t capacity = ste->capacity;

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
	ste->capacity = capacity_real;
	ste->threshold = (size_t)(capacity_real * STE_LOADFACTOR);
}

static void ste_add_child(STEntry *ste, STEntry *child)
{
	if (ste->n_children == ste->children_capacity) {
		ste->children_capacity = (ste->children_capacity * 3)/2 + 1;
		ste->children = realloc(ste->children, ste->children_capacity);
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
	free(st);
}

static void ste_free(STEntry *ste)
{
	const size_t capacity = ste->capacity;

	for (size_t i = 0; i < capacity; i++) {
		STSymbol *entry = ste->table[i];

		while (entry != NULL) {
			STSymbol *temp = entry;
			entry = entry->next;
			free(temp);
		}
	}

	free(ste->table);

	STEntry **children = ste->children;
	size_t n_children = ste->n_children;
	for (size_t i = 0; i < n_children; i++) {
		ste_free(children[i]);
	}

	free(ste->children);
	free(ste);
}
