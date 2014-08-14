#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "str.h"

typedef enum {
	NODE_EMPTY,

	NODE_INT,
	NODE_FLOAT,
	NODE_STRING,
	NODE_IDENT,

	NODE_ADD,
	NODE_SUB,
	NODE_MUL,
	NODE_DIV,
	NODE_MOD,
	NODE_POW,
	NODE_ASSIGN,

	NODE_UPLUS,
	NODE_UMINUS,

	NODE_PRINT,
	NODE_IF,
	NODE_WHILE,
	NODE_DEF,

	NODE_BLOCK,
	NODE_CALL
} NodeType;

#define AST_TYPE_ASSERT(ast, nodetype) assert((ast)->type == (nodetype))

struct ast_list;
typedef struct ast_list Block;
typedef struct ast_list Program;

/*
 * `AST` represents a single statement.
 */
typedef struct AST {
	NodeType type;

	union {
		int int_val;
		double float_val;
		Str *str_val;
		Str *ident;

		Block *block;
	} v;  // value

	struct AST *left;
	struct AST *right;
} AST;

/*
 * `Block` represents a string of statements.
 */
struct ast_list {
	AST *ast;
	struct ast_list *next;
};

AST *ast_new(void);
struct ast_list *ast_list_new(void);
void ast_list_free(struct ast_list *block);
void ast_print(AST *ast);
void ast_free(AST *ast);

#define IS_ASSIGNMENT(type) ((type) == NODE_ASSIGN)
#define IS_CALL(type) ((type) == NODE_CALL)
#define IS_EXPR_STMT(type) (IS_CALL(type) || IS_ASSIGNMENT(type))
#define IS_ASSIGNABLE(type) ((type) == NODE_IDENT)

#endif /* AST_H */
