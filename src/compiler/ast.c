#include <stdlib.h>
#include <stdio.h>
#include "err.h"
#include "util.h"
#include "ast.h"

RhoAST *rho_ast_new(RhoNodeType type, RhoAST *left, RhoAST *right, unsigned int lineno)
{
	RhoAST *ast = rho_malloc(sizeof(RhoAST));
	ast->type = type;
	ast->lineno = lineno;
	ast->left = left;
	ast->right = right;
	return ast;
}

struct rho_ast_list *rho_ast_list_new(void)
{
	struct rho_ast_list *list = rho_malloc(sizeof(struct rho_ast_list));
	list->ast = NULL;
	list->next = NULL;
	return list;
}

void rho_ast_list_free(struct rho_ast_list *list)
{
	while (list != NULL) {
		struct rho_ast_list *temp = list;
		list = list->next;
		rho_ast_free(temp->ast);
		free(temp);
	}
}

void rho_ast_free(RhoAST *ast)
{
	if (ast == NULL)
		return;

	switch (ast->type) {
	case RHO_NODE_STRING:
		rho_str_free(ast->v.str_val);
		break;
	case RHO_NODE_IDENT:
		rho_str_free(ast->v.ident);
		break;
	case RHO_NODE_IF:
	case RHO_NODE_ELIF:
	case RHO_NODE_FOR:
		rho_ast_free(ast->v.middle);
		break;
	case RHO_NODE_DEF:
	case RHO_NODE_GEN:
	case RHO_NODE_CALL:
		rho_ast_list_free(ast->v.params);
		break;
	case RHO_NODE_BLOCK:
		rho_ast_list_free(ast->v.block);
		break;
	case RHO_NODE_LIST:
	case RHO_NODE_TUPLE:
	case RHO_NODE_SET:
	case RHO_NODE_DICT:
		rho_ast_list_free(ast->v.list);
		break;
	case RHO_NODE_TRY_CATCH:
		rho_ast_list_free(ast->v.excs);
		break;
	case RHO_NODE_COND_EXPR:
		rho_ast_free(ast->v.middle);
		break;
	default:
		break;
	}

	RhoAST *left = ast->left, *right = ast->right;
	free(ast);
	rho_ast_free(left);
	rho_ast_free(right);
}
