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

static void ast_print_at(AST *ast, unsigned int level)
{
	if (ast == NULL)
		return;

	for (unsigned int i = 0; i < level; i++) {
		printf("  ");
	}

	switch (ast->type) {
	case NODE_EMPTY:
		printf(";");
		break;
	case NODE_INT:
		printf("%d", ast->v.int_val);
		break;
	case NODE_FLOAT:
		printf("%f", ast->v.float_val);
		break;
	case NODE_STRING:
		printf("%s", ast->v.str_val->value);
		break;
	case NODE_IDENT:
		printf("%s", ast->v.ident->value);
		break;
	case NODE_ADD:
		printf("+");
		break;
	case NODE_SUB:
		printf("-");
		break;
	case NODE_MUL:
		printf("*");
		break;
	case NODE_DIV:
		printf("/");
		break;
	case NODE_MOD:
		printf("%%");
		break;
	case NODE_POW:
		printf("**");
		break;
	case NODE_BITAND:
		printf("&");
		break;
	case NODE_BITOR:
		printf("|");
		break;
	case NODE_XOR:
		printf("^");
		break;
	case NODE_BITNOT:
		printf("~");
		break;
	case NODE_SHIFTL:
		printf("<<");
		break;
	case NODE_SHIFTR:
		printf(">>");
		break;
	case NODE_AND:
		printf("and");
		break;
	case NODE_OR:
		printf("or");
		break;
	case NODE_NOT:
		printf("!");
		break;
	case NODE_EQUAL:
		printf("==");
		break;
	case NODE_NOTEQ:
		printf("!=");
		break;
	case NODE_LT:
		printf("<");
		break;
	case NODE_GT:
		printf(">");
		break;
	case NODE_LE:
		printf("<=");
		break;
	case NODE_GE:
		printf(">=");
		break;
	case NODE_DOT:
		printf(".");
		break;
	case NODE_ASSIGNMENTS_START:
		printf("NODE_ASSIGNMENTS_START");
		break;
	case NODE_ASSIGN:
		printf("=");
		break;
	case NODE_ASSIGN_ADD:
		printf("+=");
		break;
	case NODE_ASSIGN_SUB:
		printf("-=");
		break;
	case NODE_ASSIGN_MUL:
		printf("*=");
		break;
	case NODE_ASSIGN_DIV:
		printf("/=");
		break;
	case NODE_ASSIGN_MOD:
		printf("%%=");
		break;
	case NODE_ASSIGN_POW:
		printf("**=");
		break;
	case NODE_ASSIGN_BITAND:
		printf("&=");
		break;
	case NODE_ASSIGN_BITOR:
		printf("|=");
		break;
	case NODE_ASSIGN_XOR:
		printf("^=");
		break;
	case NODE_ASSIGN_SHIFTL:
		printf("<<=");
		break;
	case NODE_ASSIGN_SHIFTR:
		printf(">>=");
		break;
	case NODE_ASSIGNMENTS_END:
		printf("NODE_ASSIGNMENTS_END");
		break;
	case NODE_UPLUS:
		printf("+");
		break;
	case NODE_UMINUS:
		printf("-");
		break;
	case NODE_PRINT:
		printf("print");
		break;
	case NODE_IF:
		printf("if");
		break;
	case NODE_ELIF:
		printf("elif");
		break;
	case NODE_ELSE:
		printf("else");
		break;
	case NODE_WHILE:
		printf("while");
		break;
	case NODE_DEF:
		printf("def");
		break;
	case NODE_BREAK:
		printf("break");
		break;
	case NODE_CONTINUE:
		printf("continue");
		break;
	case NODE_RETURN:
		printf("return");
		break;
	case NODE_THROW:
		printf("throw");
		break;
	case NODE_TRY_CATCH:
		printf("try ... catch");
		break;
	case NODE_BLOCK:
		printf("{}");
		break;
	case NODE_LIST:
		printf("[]");
		break;
	case NODE_TUPLE:
		printf("()");
		break;
	case NODE_CALL:
		printf("()");
		break;
	case NODE_INDEX:
		printf("[n]");
		break;
	}
	printf("\n");

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
