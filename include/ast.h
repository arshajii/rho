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
	NODE_BITAND,
	NODE_BITOR,
	NODE_XOR,
	NODE_BITNOT,
	NODE_SHIFTL,
	NODE_SHIFTR,
	NODE_AND,
	NODE_OR,
	NODE_NOT,
	NODE_EQUAL,
	NODE_NOTEQ,
	NODE_LT,
	NODE_GT,
	NODE_LE,
	NODE_GE,

	NODE_ASSIGNMENTS_START,
	NODE_ASSIGN,
	NODE_ASSIGN_ADD,
	NODE_ASSIGN_SUB,
	NODE_ASSIGN_MUL,
	NODE_ASSIGN_DIV,
	NODE_ASSIGN_MOD,
	NODE_ASSIGN_POW,
	NODE_ASSIGN_BITAND,
	NODE_ASSIGN_BITOR,
	NODE_ASSIGN_XOR,
	NODE_ASSIGN_SHIFTL,
	NODE_ASSIGN_SHIFTR,
	NODE_ASSIGNMENTS_END,

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

#define IS_ASSIGNMENT(type) (NODE_ASSIGNMENTS_START < (type) && (type) < NODE_ASSIGNMENTS_END)
#define IS_CALL(type) ((type) == NODE_CALL)
#define IS_EXPR_STMT(type) (IS_CALL(type) || IS_ASSIGNMENT(type))
#define IS_ASSIGNABLE(type) ((type) == NODE_IDENT)

#endif /* AST_H */
