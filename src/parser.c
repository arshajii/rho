#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "str.h"
#include "err.h"
#include "util.h"
#include "parser.h"

typedef struct {
	TokType type;
	unsigned int prec;  // precedence of operator
	bool assoc;         // associativity: true = left, false = right
} Op;

static Op op_from_tok_type(TokType type);
static NodeType nodetype_from_op(Op op);

const Op ops[] = {
	/*
	OP,                 PREC,        ASSOC */
	{TOK_PLUS,          70,          true},
	{TOK_MINUS,         70,          true},
	{TOK_MUL,           80,          true},
	{TOK_DIV,           80,          true},
	{TOK_MOD,           80,          true},
	{TOK_POW,           90,          false},
	{TOK_BITAND,        32,          true},
	{TOK_BITOR,         30,          true},
	{TOK_XOR,           31,          true},
	{TOK_SHIFTL,        60,          true},
	{TOK_SHIFTR,        60,          true},
	{TOK_AND,           21,          true},
	{TOK_OR,            20,          true},
	{TOK_EQUAL,         40,          true},
	{TOK_NOTEQ,         40,          true},
	{TOK_LT,            50,          true},
	{TOK_GT,            50,          true},
	{TOK_LE,            50,          true},
	{TOK_GE,            50,          true},
	{TOK_ASSIGN,        10,          true},
	{TOK_ASSIGN_ADD,    10,          true},
	{TOK_ASSIGN_SUB,    10,          true},
	{TOK_ASSIGN_MUL,    10,          true},
	{TOK_ASSIGN_DIV,    10,          true},
	{TOK_ASSIGN_MOD,    10,          true},
	{TOK_ASSIGN_POW,    10,          true},
	{TOK_ASSIGN_BITAND, 10,          true},
	{TOK_ASSIGN_BITOR,  10,          true},
	{TOK_ASSIGN_XOR,    10,          true},
	{TOK_ASSIGN_SHIFTL, 10,          true},
	{TOK_ASSIGN_SHIFTR, 10,          true},
	{TOK_DOT,           99,          true},
};

static const size_t ops_size = (sizeof(ops) / sizeof(Op));

#define FUNCTION_MAX_PARAMS 128

static AST *parse_stmt(Lexer *lex);

static AST *parse_expr(Lexer *lex);
static AST *parse_parens(Lexer *lex);
static AST *parse_expr_min_prec(Lexer *lex, unsigned int min_prec);
static AST *parse_atom(Lexer *lex);
static AST *parse_unop(Lexer *lex);

static AST *parse_int(Lexer *lex);
static AST *parse_float(Lexer *lex);
static AST *parse_str(Lexer *lex);
static AST *parse_ident(Lexer *lex);

static AST *parse_print(Lexer *lex);
static AST *parse_if(Lexer *lex);
static AST *parse_while(Lexer *lex);
static AST *parse_def(Lexer *lex);
static AST *parse_break(Lexer *lex);
static AST *parse_continue(Lexer *lex);
static AST *parse_return(Lexer *lex);
static AST *parse_throw(Lexer *lex);
static AST *parse_try_catch(Lexer *lex);

static AST *parse_block(Lexer *lex);
static AST *parse_list(Lexer *lex);

static AST *parse_empty(Lexer *lex);

static struct ast_list *parse_comma_separated_list(Lexer *lex,
                                                   const TokType open_type, const TokType close_type,
                                                   AST *(*sub_element_parse_routine)(Lexer *),
                                                   unsigned int *count);

static Token *expect(Lexer *lex, TokType type);

static void parse_err_unexpected_token(Lexer *lex, Token *tok);
static void parse_err_not_a_statement(Lexer *lex, Token *tok);
static void parse_err_unclosed(Lexer *lex, Token *tok);
static void parse_err_invalid_assign(Lexer *lex, Token *tok);
static void parse_err_invalid_break(Lexer *lex, Token *tok);
static void parse_err_invalid_continue(Lexer *lex, Token *tok);
static void parse_err_invalid_return(Lexer *lex, Token *tok);
static void parse_err_too_many_params(Lexer *lex, Token *tok);
static void parse_err_empty_catch(Lexer *lex, Token *tok);

Program *parse(Lexer *lex)
{
	Program *head = ast_list_new();
	struct ast_list *node = head;

	while (lex_has_next(lex)) {
		AST *stmt = parse_stmt(lex);

		if (stmt == NULL) {
			break;
		}

		/*
		 * We don't include empty statements
		 * in the syntax tree.
		 */
		if (stmt->type == NODE_EMPTY) {
			free(stmt);
			continue;
		}

		if (node->ast != NULL) {
			node->next = ast_list_new();
			node = node->next;
		}

		node->ast = stmt;
	}

	return head;
}

/*
 * Parses a top-level statement
 */
static AST *parse_stmt(Lexer *lex)
{
	Token *tok = lex_peek_token(lex);

	AST *stmt;

	switch (tok->type) {
	case TOK_PRINT:
		stmt = parse_print(lex);
		break;
	case TOK_IF:
		stmt = parse_if(lex);
		break;
	case TOK_WHILE:
		stmt = parse_while(lex);
		break;
	case TOK_DEF:
		stmt = parse_def(lex);
		break;
	case TOK_BREAK:
		stmt = parse_break(lex);
		break;
	case TOK_CONTINUE:
		stmt = parse_continue(lex);
		break;
	case TOK_RETURN:
		stmt = parse_return(lex);
		break;
	case TOK_THROW:
		stmt = parse_throw(lex);
		break;
	case TOK_TRY:
		stmt = parse_try_catch(lex);
		break;
	case TOK_SEMICOLON:
		return parse_empty(lex);
	case TOK_EOF:
		return NULL;
	default: {
		AST *expr_stmt = parse_expr(lex);
		const NodeType type = expr_stmt->type;

		/*
		 * Not every expression is considered a statement. For
		 * example, the expression "2 + 2" on its own does not
		 * have a useful effect and is therefore not considered
		 * a valid statement. An assignment like "a = 2", on the
		 * other hand, is considered a valid statement. We must
		 * ensure that the expression we have just parsed is a
		 * valid statement.
		 */
		if (!IS_EXPR_STMT(type)) {
			parse_err_not_a_statement(lex, tok);
		}

		stmt = expr_stmt;
	}
	}

	Token *stmt_end = lex_peek_token_direct(lex);
	const TokType stmt_end_type = stmt_end->type;

	if (stmt_end_type != TOK_SEMICOLON &&
	    stmt_end_type != TOK_NEWLINE &&
	    stmt_end_type != TOK_EOF &&
	    stmt_end_type != TOK_BRACE_CLOSE) {

		parse_err_unexpected_token(lex, stmt_end);
	}

	return stmt;
}

static AST *parse_expr(Lexer *lex)
{
	return parse_expr_min_prec(lex, 1);
}

/*
 * Implementation of precedence climbing method.
 */
static AST *parse_expr_min_prec(Lexer *lex, unsigned int min_prec)
{
	AST *lhs = parse_atom(lex);

	while (lex_has_next(lex)) {

		Token *tok = lex_peek_token(lex);

		const TokType type = tok->type;

		if (!IS_OP(type)) {
			break;
		}

		const Op op = op_from_tok_type(type);

		if (op.prec < min_prec) {
			break;
		}

		const NodeType lhs_type = lhs->type;

		if (IS_ASSIGNMENT_TOK(op.type) && (min_prec != 1 || !IS_ASSIGNABLE(lhs_type))) {
			parse_err_invalid_assign(lex, tok);
		}

		const unsigned int next_min_prec = op.assoc ? (op.prec + 1) : op.prec;

		lex_next_token(lex);

		AST *rhs = parse_expr_min_prec(lex, next_min_prec);

		NodeType node_type = nodetype_from_op(op);
		AST *ast = ast_new(node_type, lhs, rhs, tok->lineno);
		lhs = ast;
	}

	return lhs;
}

/*
 * Parses a single unit of code. One of:
 *
 *  i.   single literal
 *           i-a) int literal
 *           i-b) float literal
 *           i-c) string literal
 *  ii.  parenthesized expression
 *  iii. variable
 *  iv.  dot operation [1]
 *
 *  Atoms can also consist of multiple postfix
 *  components:
 *
 *  i.   Call (e.g. "foo(a)(b, c)")
 *  ii.  Index (e.g. "foo[a][b][c]")
 *
 *  [1] Because of the high precedence of the dot
 *  operator, `parse_atom` treats an expression
 *  of the form `a.b.c` as one atom, and would,
 *  when parsing such an expression, return the
 *  following tree:
 *
 *          .
 *         / \
 *        .   c
 *       / \
 *      a   b
 *
 *  As a second example, consider `a.b(x.y)`. In
 *  this case, the following tree would be produced
 *  and returned:
 *
 *         ( ) --------> .
 *          |           / \
 *          .          x   y
 *         / \
 *        a   b
 *
 *  The horizontal arrow represents the parameters of
 *  the function call.
 */
static AST *parse_atom(Lexer *lex)
{
	Token *tok = lex_peek_token(lex);
	AST *ast;

	switch (tok->type) {
	case TOK_PAREN_OPEN:
		ast = parse_parens(lex);
		break;
	case TOK_INT:
		ast = parse_int(lex);
		break;
	case TOK_FLOAT:
		ast = parse_float(lex);
		break;
	case TOK_STR:
		ast = parse_str(lex);
		break;
	case TOK_IDENT:
		ast = parse_ident(lex);
		break;
	case TOK_BRACK_OPEN:
		ast = parse_list(lex);
		break;
	case TOK_NOT:
	case TOK_BITNOT:
	case TOK_PLUS:
	case TOK_MINUS: {
		ast = parse_unop(lex);
		break;
	}
	default:
		parse_err_unexpected_token(lex, tok);
		ast = NULL;
	}

	if (!lex_has_next(lex)) {
		goto end;
	}

	tok = lex_peek_token(lex);

	/*
	 * Dots have high priority, so
	 * we deal with them specially.
	 */
	while (tok->type == TOK_DOT) {
		Token *dot_tok = expect(lex, TOK_DOT);

		AST *ident = parse_ident(lex);
		AST *dot = ast_new(NODE_DOT, ast, ident, dot_tok->lineno);
		ast = dot;

		tok = lex_peek_token(lex);
	}

	/*
	 * Deal with function calls and index expressions...
	 */
	while (tok->type == TOK_PAREN_OPEN || tok->type == TOK_BRACK_OPEN) {
		if (tok->type == TOK_PAREN_OPEN) {
			ParamList *params = parse_comma_separated_list(lex,
														   TOK_PAREN_OPEN, TOK_PAREN_CLOSE,
														   parse_expr,
														   NULL);
			AST *call = ast_new(NODE_CALL, ast, NULL, tok->lineno);
			call->v.params = params;
			ast = call;
		} else if (tok->type == TOK_BRACK_OPEN) {
			expect(lex, TOK_BRACK_OPEN);
			AST *index = parse_expr(lex);
			expect(lex, TOK_BRACK_CLOSE);
			AST *index_expr = ast_new(NODE_INDEX, ast, index, tok->lineno);
			ast = index_expr;
		} else {
			INTERNAL_ERROR();
		}

		tok = lex_peek_token(lex);
	}

	end:
	return ast;
}

/*
 * Parses parenthesized expression.
 */
static AST *parse_parens(Lexer *lex)
{
	Token *paren_open = expect(lex, TOK_PAREN_OPEN);
	const unsigned int lineno = paren_open->lineno;

	Token *peek = lex_peek_token(lex);

	if (peek->type == TOK_PAREN_CLOSE) {
		/* we have an empty tuple */

		expect(lex, TOK_PAREN_CLOSE);
		AST *ast = ast_new(NODE_TUPLE, NULL, NULL, lineno);
		ast->v.list = NULL;
		return ast;
	}

	/* Now we either have a regular parenthesized expression
	 * OR
	 * we have a non-empty tuple.
	 */

	AST *ast = parse_expr(lex);

	if (peek->type == TOK_COMMA) {
		/* we have a non-empty tuple */

		expect(lex, TOK_COMMA);
		struct ast_list *list_head = ast_list_new();
		list_head->ast = ast;
		struct ast_list *list = list_head;

		do {
			Token *next = lex_peek_token(lex);

			if (next->type == TOK_EOF) {
				parse_err_unclosed(lex, paren_open);
			}

			if (next->type == TOK_PAREN_CLOSE) {
				break;
			}

			list->next = ast_list_new();
			list = list->next;
			list->ast = parse_expr(lex);

			next = lex_peek_token(lex);

			if (next->type == TOK_COMMA) {
				expect(lex, TOK_COMMA);
			} else if (next->type != TOK_PAREN_CLOSE) {
				parse_err_unexpected_token(lex, next);
			}
		} while (true);

		ast = ast_new(NODE_TUPLE, NULL, NULL, lineno);
		ast->v.list = list_head;
	}

	expect(lex, TOK_PAREN_CLOSE);

	return ast;
}

static AST *parse_unop(Lexer *lex)
{
	Token *tok = lex_next_token(lex);

	NodeType type;
	switch (tok->type) {
	case TOK_PLUS:
		type = NODE_UPLUS;
		break;
	case TOK_MINUS:
		type = NODE_UMINUS;
		break;
	case TOK_BITNOT:
		type = NODE_BITNOT;
		break;
	case TOK_NOT:
		type = NODE_NOT;
		break;
	default:
		type = NODE_EMPTY;
		INTERNAL_ERROR();
		break;
	}

	AST *atom = parse_atom(lex);
	AST *ast = ast_new(type, atom, NULL, tok->lineno);
	return ast;
}

static AST *parse_int(Lexer *lex)
{
	Token *tok = expect(lex, TOK_INT);
	AST *ast = ast_new(NODE_INT, NULL, NULL, tok->lineno);
	ast->v.int_val = atoi(tok->value);
	return ast;
}

static AST *parse_float(Lexer *lex)
{
	Token *tok = expect(lex, TOK_FLOAT);
	AST *ast = ast_new(NODE_FLOAT, NULL, NULL, tok->lineno);
	ast->v.float_val = atof(tok->value);
	return ast;
}

static AST *parse_str(Lexer *lex)
{
	Token *tok = expect(lex, TOK_STR);
	AST *ast = ast_new(NODE_STRING, NULL, NULL, tok->lineno);

	// deal with quotes appropriately:
	ast->v.str_val = str_new_copy(tok->value + 1, tok->length - 2);

	return ast;
}

static AST *parse_ident(Lexer *lex)
{
	Token *tok = expect(lex, TOK_IDENT);
	AST *ast = ast_new(NODE_IDENT, NULL, NULL, tok->lineno);
	ast->v.ident = str_new_copy(tok->value, tok->length);
	return ast;
}

static AST *parse_print(Lexer *lex)
{
	Token *tok = expect(lex, TOK_PRINT);
	AST *expr = parse_expr(lex);
	AST *ast = ast_new(NODE_PRINT, expr, NULL, tok->lineno);
	return ast;
}

static AST *parse_if(Lexer *lex)
{
	Token *tok = expect(lex, TOK_IF);
	AST *condition = parse_expr(lex);
	AST *body = parse_block(lex);
	AST *ast = ast_new(NODE_IF, condition, body, tok->lineno);

	AST *else_chain_base = NULL;
	AST *else_chain_last = NULL;

	while ((tok = lex_peek_token(lex))->type == TOK_ELIF) {
		expect(lex, TOK_ELIF);
		AST *elif_condition = parse_expr(lex);
		AST *elif_body = parse_block(lex);
		AST *elif = ast_new(NODE_ELIF, elif_condition, elif_body, tok->lineno);

		if (else_chain_base == NULL) {
			else_chain_base = else_chain_last = elif;
		} else {
			else_chain_last->v.middle = elif;
			else_chain_last = elif;
		}
	}

	if ((tok = lex_peek_token(lex))->type == TOK_ELSE) {
		expect(lex, TOK_ELSE);
		AST *else_body = parse_block(lex);
		AST *else_ast = ast_new(NODE_ELSE, else_body, NULL, tok->lineno);

		if (else_chain_base == NULL) {
			else_chain_base = else_chain_last = else_ast;
		} else {
			else_chain_last->v.middle = else_ast;
			else_chain_last = else_ast;
		}
	}

	if (else_chain_last != NULL) {
		else_chain_last->v.middle = NULL;
	}

	ast->v.middle = else_chain_base;

	return ast;
}

static AST *parse_while(Lexer *lex)
{
	Token *tok = expect(lex, TOK_WHILE);
	AST *condition = parse_expr(lex);

	unsigned old_in_loop = lex->in_loop;
	lex->in_loop = 1;
	AST *body = parse_block(lex);
	lex->in_loop = old_in_loop;

	AST *ast = ast_new(NODE_WHILE, condition, body, tok->lineno);
	return ast;
}

static AST *parse_def(Lexer *lex)
{
	Token *tok = expect(lex, TOK_DEF);
	Token *name_tok = lex_peek_token(lex);
	AST *name = parse_ident(lex);

	unsigned int nargs = 0;
	ParamList *params = parse_comma_separated_list(lex,
	                                               TOK_PAREN_OPEN, TOK_PAREN_CLOSE,
	                                               parse_ident,
	                                               &nargs);

	unsigned old_in_function = lex->in_function;
	lex->in_function = 1;
	AST *body = parse_block(lex);
	lex->in_function = old_in_function;

	if (nargs > FUNCTION_MAX_PARAMS) {
		parse_err_too_many_params(lex, name_tok);
	}

	AST *ast = ast_new(NODE_DEF, name, body, tok->lineno);
	ast->v.params = params;
	return ast;
}

static AST *parse_break(Lexer *lex)
{
	Token *tok = expect(lex, TOK_BREAK);

	if (!lex->in_loop) {
		parse_err_invalid_break(lex, tok);
	}

	AST *ast = ast_new(NODE_BREAK, NULL, NULL, tok->lineno);
	return ast;
}

static AST *parse_continue(Lexer *lex)
{
	Token *tok = expect(lex, TOK_CONTINUE);

	if (!lex->in_loop) {
		parse_err_invalid_continue(lex, tok);
	}

	AST *ast = ast_new(NODE_CONTINUE, NULL, NULL, tok->lineno);
	return ast;
}

static AST *parse_return(Lexer *lex)
{
	Token *tok = expect(lex, TOK_RETURN);

	if (!lex->in_function) {
		parse_err_invalid_return(lex, tok);
	}

	AST *expr = parse_expr(lex);
	AST *ast = ast_new(NODE_RETURN, expr, NULL, tok->lineno);
	return ast;
}

static AST *parse_throw(Lexer *lex)
{
	Token *tok = expect(lex, TOK_THROW);
	AST *expr = parse_expr(lex);
	AST *ast = ast_new(NODE_THROW, expr, NULL, tok->lineno);
	return ast;
}

static AST *parse_try_catch(Lexer *lex)
{
	Token *tok = expect(lex, TOK_TRY);
	AST *try_body = parse_block(lex);
	Token *catch = expect(lex, TOK_CATCH);
	unsigned int count;
	struct ast_list *exc_list = parse_comma_separated_list(lex,
	                                                       TOK_PAREN_OPEN,
	                                                       TOK_PAREN_CLOSE,
	                                                       parse_expr,
	                                                       &count);

	if (count == 0) {
		parse_err_empty_catch(lex, catch);
	}

	AST *catch_body = parse_block(lex);
	AST *ast = ast_new(NODE_TRY_CATCH, try_body, catch_body, tok->lineno);
	ast->v.excs = exc_list;
	return ast;
}

static AST *parse_block(Lexer *lex)
{
	Block *block_head = NULL;

	Token *peek = lex_peek_token(lex);

	Token *brace_open;

	if (peek->type == TOK_COLON) {
		brace_open = expect(lex, TOK_COLON);
		block_head = ast_list_new();
		block_head->ast = parse_stmt(lex);
	} else {
		brace_open = expect(lex, TOK_BRACE_OPEN);
		Block *block = NULL;

		do {
			Token *next = lex_peek_token(lex);

			if (next->type == TOK_EOF) {
				parse_err_unclosed(lex, brace_open);
			}

			if (next->type == TOK_BRACE_CLOSE) {
				break;
			}

			AST *stmt = parse_stmt(lex);

			/*
			 * We don't include empty statements
			 * in the syntax tree.
			 */
			if (stmt->type == NODE_EMPTY) {
				free(stmt);
				continue;
			}

			if (block_head == NULL) {
				block_head = ast_list_new();
				block = block_head;
			}

			if (block->ast != NULL) {
				block->next = ast_list_new();
				block = block->next;
			}

			block->ast = stmt;
		} while (true);

		expect(lex, TOK_BRACE_CLOSE);
	}

	AST *ast = ast_new(NODE_BLOCK, NULL, NULL, brace_open->lineno);
	ast->v.block = block_head;
	return ast;
}

static AST *parse_list(Lexer *lex)
{
	Token *brack_open = lex_peek_token(lex);  /* parse_comma_separated_list does error checking */
	struct ast_list *list_head = parse_comma_separated_list(lex,
	                                                        TOK_BRACK_OPEN,
	                                                        TOK_BRACK_CLOSE,
	                                                        parse_expr,
	                                                        NULL);
	AST *ast = ast_new(NODE_LIST, NULL, NULL, brack_open->lineno);
	ast->v.list = list_head;
	return ast;
}

static AST *parse_empty(Lexer *lex)
{
	Token *tok = expect(lex, TOK_SEMICOLON);
	AST *ast = ast_new(NODE_EMPTY, NULL, NULL, tok->lineno);
	return ast;
}

/*
 * Parses generic comma-separated list with given start and end delimiters.
 * Returns the number of elements in this parsed list.
 */
static struct ast_list *parse_comma_separated_list(Lexer *lex,
                                                   const TokType open_type, const TokType close_type,
                                                   AST *(*sub_element_parse_routine)(Lexer *),
                                                   unsigned int *count)
{
	Token *tok_open = expect(lex, open_type);

	struct ast_list *list_head = NULL;
	struct ast_list *list = NULL;

	unsigned int nelements = 0;

	do {
		Token *next = lex_peek_token(lex);

		if (next->type == TOK_EOF) {
			parse_err_unclosed(lex, tok_open);
		}

		if (next->type == close_type) {
			break;
		}

		if (list_head == NULL) {
			list_head = ast_list_new();
			list = list_head;
		}

		if (list->ast != NULL) {
			list->next = ast_list_new();
			list = list->next;
		}

		list->ast = sub_element_parse_routine(lex);
		++nelements;

		next = lex_peek_token(lex);

		if (next->type == TOK_COMMA) {
			expect(lex, TOK_COMMA);
		} else if (next->type != close_type) {
			parse_err_unexpected_token(lex, next);
		}
	} while (true);

	expect(lex, close_type);

	if (count != NULL) {
		*count = nelements;
	}

	return list_head;
}

static Op op_from_tok_type(TokType type)
{
	for (size_t i = 0; i < ops_size; i++) {
		if (type == ops[i].type) {
			return ops[i];
		}
	}

	INTERNAL_ERROR();
	static const Op sentinel;
	return sentinel;
}

static NodeType nodetype_from_op(Op op)
{
	switch (op.type) {
	case TOK_PLUS:
		return NODE_ADD;
	case TOK_MINUS:
		return NODE_SUB;
	case TOK_MUL:
		return NODE_MUL;
	case TOK_DIV:
		return NODE_DIV;
	case TOK_MOD:
		return NODE_MOD;
	case TOK_POW:
		return NODE_POW;
	case TOK_BITAND:
		return NODE_BITAND;
	case TOK_BITOR:
		return NODE_BITOR;
	case TOK_XOR:
		return NODE_XOR;
	case TOK_BITNOT:
		return NODE_BITNOT;
	case TOK_SHIFTL:
		return NODE_SHIFTL;
	case TOK_SHIFTR:
		return NODE_SHIFTR;
	case TOK_AND:
		return NODE_AND;
	case TOK_OR:
		return NODE_OR;
	case TOK_NOT:
		return NODE_NOT;
	case TOK_EQUAL:
		return NODE_EQUAL;
	case TOK_NOTEQ:
		return NODE_NOTEQ;
	case TOK_LT:
		return NODE_LT;
	case TOK_GT:
		return NODE_GT;
	case TOK_LE:
		return NODE_LE;
	case TOK_GE:
		return NODE_GE;
	case TOK_DOT:
		return NODE_DOT;
	case TOK_ASSIGN:
		return NODE_ASSIGN;
	case TOK_ASSIGN_ADD:
		return NODE_ASSIGN_ADD;
	case TOK_ASSIGN_SUB:
		return NODE_ASSIGN_SUB;
	case TOK_ASSIGN_MUL:
		return NODE_ASSIGN_MUL;
	case TOK_ASSIGN_DIV:
		return NODE_ASSIGN_DIV;
	case TOK_ASSIGN_MOD:
		return NODE_ASSIGN_MOD;
	case TOK_ASSIGN_POW:
		return NODE_ASSIGN_POW;
	case TOK_ASSIGN_BITAND:
		return NODE_ASSIGN_BITAND;
	case TOK_ASSIGN_BITOR:
		return NODE_ASSIGN_BITOR;
	case TOK_ASSIGN_XOR:
		return NODE_ASSIGN_XOR;
	case TOK_ASSIGN_SHIFTL:
		return NODE_ASSIGN_SHIFTL;
	case TOK_ASSIGN_SHIFTR:
		return NODE_ASSIGN_SHIFTR;
	default:
		INTERNAL_ERROR();
		return -1;
	}
}

static Token *expect(Lexer *lex, TokType type)
{
	assert(type != TOK_NONE);
	Token *next = lex_has_next(lex) ? lex_next_token(lex) : NULL;
	const TokType next_type = next ? next->type : TOK_NONE;

	if (next_type != type) {
		parse_err_unexpected_token(lex, next);
	}

	return next;
}

/*
 * Parser error functions
 */

DECL_MIN_FUNC(min, size_t)

static void err_on_tok(Lexer *lex, Token *tok)
{
	err_on_char(tok->value, lex->code, lex->end, tok->lineno);
}

static void parse_err_unexpected_token(Lexer *lex, Token *tok)
{
#define MAX_LEN 1024

	if (tok->type == TOK_EOF) {
		fprintf(stderr,
		        SYNTAX_ERROR " unexpected end-of-file after token\n\n",
		        lex->name,
		        tok->lineno);

		/*
		 * This should really always be true,
		 * since we shouldn't have an unexpected
		 * token in an empty file.
		 */
		if (lex->tok_count > 1) {
			Token *tokens = lex->tokens;
			size_t last = lex->tok_count - 2;

			while (last > 0 && tokens[last].type == TOK_NEWLINE) {
				--last;
			}

			if (tokens[last].type != TOK_NEWLINE) {
				err_on_tok(lex, &tokens[last]);
			}
		}
	} else {
		char tok_str[MAX_LEN];

		const size_t tok_len = min(tok->length, MAX_LEN - 1);
		memcpy(tok_str, tok->value, tok_len);
		tok_str[tok_len] = '\0';

		fprintf(stderr,
		        SYNTAX_ERROR " unexpected token: %s\n\n",
		        lex->name,
		        tok->lineno,
		        tok_str);

		err_on_tok(lex, tok);
	}

	exit(EXIT_FAILURE);

#undef MAX_LEN
}

static void parse_err_not_a_statement(Lexer *lex, Token *tok)
{
	fprintf(stderr,
	        SYNTAX_ERROR " not a statement\n\n",
	        lex->name,
	        tok->lineno);

	err_on_tok(lex, tok);

	exit(EXIT_FAILURE);
}

static void parse_err_unclosed(Lexer *lex, Token *tok)
{
	fprintf(stderr,
	        SYNTAX_ERROR " unclosed\n\n",
	        lex->name,
	        tok->lineno);

	err_on_tok(lex, tok);

	exit(EXIT_FAILURE);
}

static void parse_err_invalid_assign(Lexer *lex, Token *tok)
{
	fprintf(stderr,
	        SYNTAX_ERROR " misplaced assignment\n\n",
	        lex->name,
	        tok->lineno);

	err_on_tok(lex, tok);

	exit(EXIT_FAILURE);
}

static void parse_err_invalid_break(Lexer *lex, Token *tok)
{
	fprintf(stderr,
	        SYNTAX_ERROR " misplaced break statement\n\n",
	        lex->name,
	        tok->lineno);

	err_on_tok(lex, tok);

	exit(EXIT_FAILURE);
}

static void parse_err_invalid_continue(Lexer *lex, Token *tok)
{
	fprintf(stderr,
	        SYNTAX_ERROR " misplaced continue statement\n\n",
	        lex->name,
	        tok->lineno);

	err_on_tok(lex, tok);

	exit(EXIT_FAILURE);
}

static void parse_err_invalid_return(Lexer *lex, Token *tok)
{
	fprintf(stderr,
	        SYNTAX_ERROR " misplaced return statement\n\n",
	        lex->name,
	        tok->lineno);

	err_on_tok(lex, tok);

	exit(EXIT_FAILURE);
}

static void parse_err_too_many_params(Lexer *lex, Token *tok)
{
	fprintf(stderr,
	        SYNTAX_ERROR " function has too many parameters (max %d)\n\n",
	        lex->name,
	        tok->lineno,
	        FUNCTION_MAX_PARAMS);

	err_on_tok(lex, tok);

	exit(EXIT_FAILURE);
}

static void parse_err_empty_catch(Lexer *lex, Token *tok)
{
	fprintf(stderr,
	        SYNTAX_ERROR " empty catch statement\n\n",
	        lex->name,
	        tok->lineno);

	err_on_tok(lex, tok);

	exit(EXIT_FAILURE);
}
