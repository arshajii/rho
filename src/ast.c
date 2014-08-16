#include <stdlib.h>
#include <stdio.h>
#include "ast.h"

AST *ast_new(void)
{
	AST *ast = malloc(sizeof(AST));
	return ast;
}

struct ast_list *ast_list_new(void)
{
	struct ast_list *list = malloc(sizeof(Block));
	list->ast = NULL;
	list->next = NULL;
	return list;
}

static void ast_print_at(AST *ast, unsigned int level)
{
	if (ast == NULL)
		return;

	for (unsigned int i = 0; i < level; i++) {
		printf("  ");
	}

	switch (ast->type) {
	case NODE_EMPTY:
		printf(";\n");
		break;
	case NODE_INT:
		printf("%d\n", ast->v.int_val);
		break;
	case NODE_FLOAT:
		printf("%f\n", ast->v.float_val);
		break;
	case NODE_STRING:
		break;
	case NODE_IDENT:
		break;
	case NODE_ADD:
		printf("+\n");
		break;
	case NODE_SUB:
		printf("-\n");
		break;
	case NODE_MUL:
		printf("*\n");
		break;
	case NODE_DIV:
		printf("/\n");
		break;
	case NODE_MOD:
		printf("%%\n");
		break;
	case NODE_POW:
		printf("**\n");
		break;
	case NODE_BITAND:
		printf("&\n");
		break;
	case NODE_BITOR:
		printf("|\n");
		break;
	case NODE_XOR:
		printf("^\n");
		break;
	case NODE_BITNOT:
		printf("~\n");
		break;
	case NODE_SHIFTL:
		printf("<<\n");
		break;
	case NODE_SHIFTR:
		printf(">>\n");
		break;
	case NODE_AND:
		printf("and\n");
		break;
	case NODE_OR:
		printf("or\n");
		break;
	case NODE_NOT:
		printf("!\n");
		break;
	case NODE_EQUAL:
		printf("==\n");
		break;
	case NODE_NOTEQ:
		printf("!=\n");
		break;
	case NODE_LT:
		printf("<\n");
		break;
	case NODE_GT:
		printf(">\n");
		break;
	case NODE_LE:
		printf("<=\n");
		break;
	case NODE_GE:
		printf(">=\n");
		break;
	case NODE_ASSIGN:
		printf("=\n");
		break;
	case NODE_UPLUS:
		printf("u+\n");
		break;
	case NODE_UMINUS:
		printf("u-\n");
		break;
	case NODE_PRINT:
		printf("print\n");
		break;
	case NODE_IF:
		printf("if\n");
		break;
	case NODE_WHILE:
		printf("while\n");
		break;
	case NODE_DEF:
		printf("def\n");
		break;
	case NODE_BLOCK:
		printf("{}\n");
		break;
	case NODE_CALL:
		printf("call\n");
		break;
	}

	ast_print_at(ast->left, level + 1);
	ast_print_at(ast->right, level + 1);
}

void ast_print(AST *ast)
{
	ast_print_at(ast, 0);
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
	case NODE_BLOCK: {
		ast_list_free(ast->v.block);
		break;
	}
	default:
		break;
	}

	AST *left = ast->left, *right = ast->right;
	free(ast);
	ast_free(left);
	ast_free(right);
}
