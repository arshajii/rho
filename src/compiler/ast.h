#ifndef RHO_AST_H
#define RHO_AST_H

#include <stdio.h>
#include "str.h"

typedef enum {
	RHO_NODE_EMPTY,

	RHO_NODE_INT,
	RHO_NODE_FLOAT,
	RHO_NODE_STRING,
	RHO_NODE_IDENT,

	RHO_NODE_ADD,
	RHO_NODE_SUB,
	RHO_NODE_MUL,
	RHO_NODE_DIV,
	RHO_NODE_MOD,
	RHO_NODE_POW,
	RHO_NODE_BITAND,
	RHO_NODE_BITOR,
	RHO_NODE_XOR,
	RHO_NODE_BITNOT,
	RHO_NODE_SHIFTL,
	RHO_NODE_SHIFTR,
	RHO_NODE_AND,
	RHO_NODE_OR,
	RHO_NODE_NOT,
	RHO_NODE_EQUAL,
	RHO_NODE_NOTEQ,
	RHO_NODE_LT,
	RHO_NODE_GT,
	RHO_NODE_LE,
	RHO_NODE_GE,
	RHO_NODE_APPLY,
	RHO_NODE_DOT,
	RHO_NODE_DOTDOT,
	RHO_NODE_COND_EXPR,

	RHO_NODE_ASSIGNMENTS_START,
	RHO_NODE_ASSIGN,
	RHO_NODE_ASSIGN_ADD,
	RHO_NODE_ASSIGN_SUB,
	RHO_NODE_ASSIGN_MUL,
	RHO_NODE_ASSIGN_DIV,
	RHO_NODE_ASSIGN_MOD,
	RHO_NODE_ASSIGN_POW,
	RHO_NODE_ASSIGN_BITAND,
	RHO_NODE_ASSIGN_BITOR,
	RHO_NODE_ASSIGN_XOR,
	RHO_NODE_ASSIGN_SHIFTL,
	RHO_NODE_ASSIGN_SHIFTR,
	RHO_NODE_ASSIGN_APPLY,
	RHO_NODE_ASSIGNMENTS_END,

	RHO_NODE_UPLUS,
	RHO_NODE_UMINUS,

	RHO_NODE_NULL,
	RHO_NODE_PRINT,
	RHO_NODE_IF,
	RHO_NODE_ELIF,
	RHO_NODE_ELSE,
	RHO_NODE_WHILE,
	RHO_NODE_FOR,
	RHO_NODE_IN,
	RHO_NODE_DEF,
	RHO_NODE_GEN,
	RHO_NODE_BREAK,
	RHO_NODE_CONTINUE,
	RHO_NODE_RETURN,
	RHO_NODE_THROW,
	RHO_NODE_PRODUCE,
	RHO_NODE_TRY_CATCH,
	RHO_NODE_IMPORT,
	RHO_NODE_EXPORT,

	RHO_NODE_BLOCK,
	RHO_NODE_LIST,
	RHO_NODE_TUPLE,
	RHO_NODE_SET,
	RHO_NODE_DICT,
	RHO_NODE_LAMBDA,

	RHO_NODE_CALL,
	RHO_NODE_INDEX,
	RHO_NODE_DICT_ELEM,
} RhoNodeType;

#define RHO_AST_TYPE_ASSERT(ast, nodetype) assert((ast)->type == (nodetype))

struct rho_ast_list;
typedef struct rho_ast_list RhoProgram;
typedef struct rho_ast_list RhoBlock;
typedef struct rho_ast_list RhoParamList;
typedef struct rho_ast_list RhoExcList;

/*
 * Fundamental syntax tree unit
 */
typedef struct rho_ast {
	RhoNodeType type;
	unsigned int lineno;

	union {
		int int_val;
		double float_val;
		RhoStr *str_val;
		RhoStr *ident;
		struct rho_ast *middle;
		RhoBlock *block;
		RhoParamList *params;
		RhoExcList *excs;
		struct rho_ast_list *list;
		unsigned int max_dollar_ident;
	} v;

	struct rho_ast *left;
	struct rho_ast *right;
} RhoAST;

/*
 * `Block` represents a string of statements.
 */
struct rho_ast_list {
	RhoAST *ast;
	struct rho_ast_list *next;
};

RhoAST *rho_ast_new(RhoNodeType type, RhoAST *left, RhoAST *right, unsigned int lineno);
struct rho_ast_list *rho_ast_list_new(void);
void rho_ast_list_free(struct rho_ast_list *block);
void rho_ast_free(RhoAST *ast);

#define RHO_NODE_TYPE_IS_ASSIGNMENT(type) (RHO_NODE_ASSIGNMENTS_START < (type) && (type) < RHO_NODE_ASSIGNMENTS_END)
#define RHO_NODE_TYPE_IS_CALL(type)       ((type) == RHO_NODE_CALL)
#define RHO_NODE_TYPE_IS_EXPR_STMT(type)  (RHO_NODE_TYPE_IS_CALL(type) || RHO_NODE_TYPE_IS_ASSIGNMENT(type))
#define RHO_NODE_TYPE_IS_ASSIGNABLE(type) ((type) == RHO_NODE_IDENT || (type) == RHO_NODE_DOT || (type) == RHO_NODE_INDEX)

#endif /* RHO_AST_H */
