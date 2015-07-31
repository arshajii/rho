#include <stdlib.h>
#include <stdio.h>
#include "err.h"
#include "util.h"
#include "ast.h"

AST *ast_new(NodeType type, AST *left, AST *right, unsigned int lineno)
{
	AST *ast = rho_malloc(sizeof(AST));
	ast->type = type;
	ast->lineno = lineno;
	ast->left = left;
	ast->right = right;
	return ast;
}

struct ast_list *ast_list_new(void)
{
	struct ast_list *list = rho_malloc(sizeof(struct ast_list));
	list->ast = NULL;
	list->next = NULL;
	return list;
}

void ast_list_free(struct ast_list *list)
{
	while (list != NULL) {
		struct ast_list *temp = list;
		list = list->next;
		ast_free(temp->ast);
		free(temp);
	}
}

void ast_free(AST *ast)
{
	if (ast == NULL)
		return;

	switch (ast->type) {
	case NODE_STRING:
		str_free(ast->v.str_val);
		break;
	case NODE_IDENT:
		str_free(ast->v.ident);
		break;
	case NODE_IF:
	case NODE_ELIF:
		ast_free(ast->v.middle);
		break;
	case NODE_DEF:
	case NODE_CALL:
		ast_list_free(ast->v.params);
		break;
	case NODE_BLOCK:
		ast_list_free(ast->v.block);
		break;
	case NODE_LIST:
	case NODE_TUPLE:
		ast_list_free(ast->v.list);
		break;
	case NODE_TRY_CATCH:
		ast_list_free(ast->v.excs);
		break;
	default:
		break;
	}

	AST *left = ast->left, *right = ast->right;
	free(ast);
	ast_free(left);
	ast_free(right);
}
