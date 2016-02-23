#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "str.h"
#include "err.h"
#include "util.h"
#include "lexer.h"
#include "parser.h"

#define ERROR_CHECK(p) do { if (PARSER_ERROR(p)) return NULL; } while (0)

#define ERROR_CHECK_AST(p, should_be_null, free_me) \
	do { \
		if (PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			ast_free(free_me); \
			return NULL; \
		} \
	} while (0)

#define ERROR_CHECK_AST2(p, should_be_null, free_me1, free_me2) \
	do { \
		if (PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			ast_free(free_me1); \
			ast_free(free_me2); \
			return NULL; \
		} \
	} while (0)

#define ERROR_CHECK_AST3(p, should_be_null, free_me1, free_me2, free_me3) \
	do { \
		if (PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			ast_free(free_me1); \
			ast_free(free_me2); \
			ast_free(free_me3); \
			return NULL; \
		} \
	} while (0)

#define ERROR_CHECK_LIST(p, should_be_null, free_me) \
	do { \
		if (PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			ast_list_free(free_me); \
			return NULL; \
		} \
	} while (0)

typedef struct {
	TokType type;
	unsigned int prec;  // precedence of operator
	bool assoc;         // associativity: true = left, false = right
} Op;

static Op op_from_tok_type(TokType type);
static NodeType nodetype_from_op(Op op);

static const Op ops[] = {
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
	{TOK_ASSIGN_AT,     10,          true},
	{TOK_DOT,           99,          true},
	{TOK_DOTDOT,        92,          true},
	{TOK_AT,            91,          false},
	{TOK_IN,             9,          true},
	{TOK_IF,            22,          true},  /* ternary operator */
};

static const size_t ops_size = (sizeof(ops) / sizeof(Op));

#define FUNCTION_MAX_PARAMS 128

static AST *parse_stmt(Parser *p);

static AST *parse_expr(Parser *p);
static AST *parse_expr_no_assign(Parser *p);
static AST *parse_parens(Parser *p);
static AST *parse_atom(Parser *p);
static AST *parse_unop(Parser *p);

static AST *parse_int(Parser *p);
static AST *parse_float(Parser *p);
static AST *parse_str(Parser *p);
static AST *parse_ident(Parser *p);
static AST *parse_dollar_ident(Parser *p);

static AST *parse_print(Parser *p);
static AST *parse_if(Parser *p);
static AST *parse_while(Parser *p);
static AST *parse_for(Parser *p);
static AST *parse_def(Parser *p);
static AST *parse_break(Parser *p);
static AST *parse_continue(Parser *p);
static AST *parse_return(Parser *p);
static AST *parse_throw(Parser *p);
static AST *parse_try_catch(Parser *p);
static AST *parse_import(Parser *p);
static AST *parse_export(Parser *p);

static AST *parse_block(Parser *p);
static AST *parse_list(Parser *p);
static AST *parse_lambda(Parser *p);

static AST *parse_empty(Parser *p);

static struct ast_list *parse_comma_separated_list(Parser *p,
                                                   const TokType open_type, const TokType close_type,
                                                   AST *(*sub_element_parse_routine)(Parser *),
                                                   unsigned int *count);

static Token *expect(Parser *p, TokType type);

static void parse_err_unexpected_token(Parser *p, Token *tok);
static void parse_err_not_a_statement(Parser *p, Token *tok);
static void parse_err_unclosed(Parser *p, Token *tok);
static void parse_err_invalid_assign(Parser *p, Token *tok);
static void parse_err_invalid_break(Parser *p, Token *tok);
static void parse_err_invalid_continue(Parser *p, Token *tok);
static void parse_err_invalid_return(Parser *p, Token *tok);
static void parse_err_too_many_params(Parser *p, Token *tok);
static void parse_err_dup_params(Parser *p, Token *tok, const char *param);
static void parse_err_non_default_after_default(Parser *p, Token *tok);
static void parse_err_malformed_params(Parser *p, Token *tok);
static void parse_err_too_many_args(Parser *p, Token *tok);
static void parse_err_dup_named_args(Parser *p, Token *tok, const char *name);
static void parse_err_unnamed_after_named(Parser *p, Token *tok);
static void parse_err_malformed_args(Parser *p, Token *tok);
static void parse_err_empty_catch(Parser *p, Token *tok);
static void parse_err_misplaced_dollar_identifier(Parser *p, Token *tok);

Parser *parser_new(char *str, const char *name)
{
#define INITIAL_TOKEN_ARRAY_CAPACITY 5
	Parser *p = rho_malloc(sizeof(Parser));
	p->code = str;
	p->end = &str[strlen(str) - 1];
	p->pos = &str[0];
	p->mark = 0;
	p->tokens = rho_malloc(INITIAL_TOKEN_ARRAY_CAPACITY * sizeof(Token));
	p->tok_count = 0;
	p->tok_capacity = INITIAL_TOKEN_ARRAY_CAPACITY;
	p->tok_pos = 0;
	p->lineno = 1;
	p->peek = NULL;
	p->name = name;
	p->in_function = 0;
	p->in_lambda = 0;
	p->in_loop = 0;
	p->error_type = PARSE_ERR_NONE;
	p->error_msg = NULL;

	parser_tokenize(p);

	return p;
#undef INITIAL_TOKEN_ARRAY_CAPACITY
}

void parser_free(Parser *p)
{
	free(p->tokens);
	FREE(p->error_msg);
	free(p);
}

Program *parse(Parser *p)
{
	Program *head = ast_list_new();
	struct ast_list *node = head;

	while (parser_has_next_token(p)) {
		AST *stmt = parse_stmt(p);
		ERROR_CHECK_LIST(p, stmt, head);

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
static AST *parse_stmt(Parser *p)
{
	Token *tok = parser_peek_token(p);

	AST *stmt;

	switch (tok->type) {
	case TOK_PRINT:
		stmt = parse_print(p);
		break;
	case TOK_IF:
		stmt = parse_if(p);
		break;
	case TOK_WHILE:
		stmt = parse_while(p);
		break;
	case TOK_FOR:
		stmt = parse_for(p);
		break;
	case TOK_DEF:
		stmt = parse_def(p);
		break;
	case TOK_BREAK:
		stmt = parse_break(p);
		break;
	case TOK_CONTINUE:
		stmt = parse_continue(p);
		break;
	case TOK_RETURN:
		stmt = parse_return(p);
		break;
	case TOK_THROW:
		stmt = parse_throw(p);
		break;
	case TOK_TRY:
		stmt = parse_try_catch(p);
		break;
	case TOK_IMPORT:
		stmt = parse_import(p);
		break;
	case TOK_EXPORT:
		stmt = parse_export(p);
		break;
	case TOK_SEMICOLON:
		return parse_empty(p);
	case TOK_EOF:
		return NULL;
	default: {
		AST *expr_stmt = parse_expr(p);
		ERROR_CHECK(p);
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
			parse_err_not_a_statement(p, tok);
			ast_free(expr_stmt);
			return NULL;
		}

		stmt = expr_stmt;
	}
	}
	ERROR_CHECK(p);

	Token *stmt_end = parser_peek_token_direct(p);
	const TokType stmt_end_type = stmt_end->type;

	if (stmt_end_type != TOK_SEMICOLON &&
	    stmt_end_type != TOK_NEWLINE &&
	    stmt_end_type != TOK_EOF &&
	    stmt_end_type != TOK_BRACE_CLOSE) {

		parse_err_unexpected_token(p, stmt_end);
		ast_free(stmt);
		return NULL;
	}

	return stmt;
}

static AST *parse_expr_helper(Parser *p, const bool allow_assigns);

static AST *parse_expr_min_prec(Parser *p, unsigned int min_prec, bool allow_assigns);

static AST *parse_expr(Parser *p)
{
	return parse_expr_helper(p, true);
}

static AST *parse_expr_no_assign(Parser *p)
{
	return parse_expr_helper(p, false);
}

static AST *parse_expr_helper(Parser *p, const bool allow_assigns)
{
	return parse_expr_min_prec(p, 1, allow_assigns);
}

/*
 * Implementation of precedence climbing method.
 */
static AST *parse_expr_min_prec(Parser *p, unsigned int min_prec, bool allow_assigns)
{
	AST *lhs = parse_atom(p);
	ERROR_CHECK(p);

	while (parser_has_next_token(p)) {

		Token *tok = parser_peek_token(p);

		const TokType type = tok->type;

		if (!IS_OP(type)) {
			break;
		}

		const Op op = op_from_tok_type(type);

		if (op.prec < min_prec) {
			break;
		}

		if (IS_ASSIGNMENT_TOK(op.type) &&
		    (!allow_assigns || min_prec != 1 || !IS_ASSIGNABLE(lhs->type))) {
			parse_err_invalid_assign(p, tok);
			ast_free(lhs);
			return NULL;
		}

		const unsigned int next_min_prec = op.assoc ? (op.prec + 1) : op.prec;

		parser_next_token(p);

		const bool ternary = (op.type == TOK_IF);
		AST *cond = NULL;

		if (ternary) {  /* ternary operator */
			cond = parse_expr_no_assign(p);
			expect(p, TOK_ELSE);
			ERROR_CHECK_AST2(p, NULL, cond, lhs);
		}

		AST *rhs = parse_expr_min_prec(p, next_min_prec, false);
		ERROR_CHECK_AST2(p, rhs, lhs, cond);

		NodeType node_type = nodetype_from_op(op);
		AST *ast = ast_new(node_type, lhs, rhs, tok->lineno);

		if (ternary) {
			ast->v.middle = cond;
		}

		lhs = ast;
		allow_assigns = false;
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
static AST *parse_atom(Parser *p)
{
	Token *tok = parser_peek_token(p);
	AST *ast;

	switch (tok->type) {
	case TOK_PAREN_OPEN:
		ast = parse_parens(p);
		break;
	case TOK_INT:
		ast = parse_int(p);
		break;
	case TOK_FLOAT:
		ast = parse_float(p);
		break;
	case TOK_STR:
		ast = parse_str(p);
		break;
	case TOK_IDENT:
		ast = parse_ident(p);
		break;
	case TOK_DOLLAR:
		ast = parse_dollar_ident(p);
		break;
	case TOK_BRACK_OPEN:
		ast = parse_list(p);
		break;
	case TOK_NOT:
	case TOK_BITNOT:
	case TOK_PLUS:
	case TOK_MINUS:
		ast = parse_unop(p);
		break;
	case TOK_COLON:
		ast = parse_lambda(p);
		break;
	default:
		parse_err_unexpected_token(p, tok);
		ast = NULL;
		return NULL;
	}

	ERROR_CHECK(p);

	if (!parser_has_next_token(p)) {
		goto end;
	}

	tok = parser_peek_token(p);

	/*
	 * Deal with cases like `foo[7].bar(42)`...
	 */
	while (tok->type == TOK_DOT || tok->type == TOK_PAREN_OPEN || tok->type == TOK_BRACK_OPEN) {
		switch (tok->type) {
		case TOK_DOT: {
			Token *dot_tok = expect(p, TOK_DOT);
			ERROR_CHECK_AST(p, dot_tok, ast);
			AST *ident = parse_ident(p);
			ERROR_CHECK_AST(p, ident, ast);
			AST *dot = ast_new(NODE_DOT, ast, ident, dot_tok->lineno);
			ast = dot;
			break;
		}
		case TOK_PAREN_OPEN: {
			unsigned int nargs;
			ParamList *params = parse_comma_separated_list(p,
			                                               TOK_PAREN_OPEN, TOK_PAREN_CLOSE,
			                                               parse_expr,
			                                               &nargs);

			ERROR_CHECK_AST(p, params, ast);

			/* argument syntax check */
			for (struct ast_list *param = params; param != NULL; param = param->next) {
				if (IS_ASSIGNMENT(param->ast->type) &&
				      (param->ast->type != NODE_ASSIGN ||
				       param->ast->left->type != NODE_IDENT)) {
					parse_err_malformed_args(p, tok);
					ast_list_free(params);
					ast_free(ast);
					return NULL;
				}
			}

			if (nargs > FUNCTION_MAX_PARAMS) {
				parse_err_too_many_args(p, tok);
				ast_list_free(params);
				ast_free(ast);
				return NULL;
			}

			/* no unnamed arguments after named ones */
			bool named = false;
			for (struct ast_list *param = params; param != NULL; param = param->next) {
				if (param->ast->type == NODE_ASSIGN) {
					named = true;
				} else if (named) {
					parse_err_unnamed_after_named(p, tok);
					ast_list_free(params);
					ast_free(ast);
					return NULL;
				}
			}

			/* no duplicate named arguments */
			for (struct ast_list *param = params; param != NULL; param = param->next) {
				if (param->ast->type != NODE_ASSIGN) {
					continue;
				}
				AST *ast1 = param->ast->left;

				for (struct ast_list *check = params; check != param; check = check->next) {
					if (check->ast->type != NODE_ASSIGN) {
						continue;
					}
					AST *ast2 = check->ast->left;

					if (str_eq(ast1->v.ident, ast2->v.ident)) {
						parse_err_dup_named_args(p, tok, ast1->v.ident->value);
						ast_list_free(params);
						ast_free(ast);
						return NULL;
					}
				}
			}

			AST *call = ast_new(NODE_CALL, ast, NULL, tok->lineno);
			call->v.params = params;
			ast = call;
			break;
		}
		case TOK_BRACK_OPEN: {
			expect(p, TOK_BRACK_OPEN);
			ERROR_CHECK_AST(p, NULL, ast);
			AST *index = parse_expr_no_assign(p);
			ERROR_CHECK_AST(p, index, ast);
			expect(p, TOK_BRACK_CLOSE);
			ERROR_CHECK_AST2(p, NULL, ast, index);
			AST *index_expr = ast_new(NODE_INDEX, ast, index, tok->lineno);
			ast = index_expr;
			break;
		}
		default: {
			INTERNAL_ERROR();
		}
		}

		tok = parser_peek_token(p);
	}

	end:
	return ast;
}

/*
 * Parses parenthesized expression.
 */
static AST *parse_parens(Parser *p)
{
	Token *paren_open = expect(p, TOK_PAREN_OPEN);
	ERROR_CHECK(p);
	const unsigned int lineno = paren_open->lineno;

	Token *peek = parser_peek_token(p);

	if (peek->type == TOK_PAREN_CLOSE) {
		/* we have an empty tuple */

		expect(p, TOK_PAREN_CLOSE);
		ERROR_CHECK(p);
		AST *ast = ast_new(NODE_TUPLE, NULL, NULL, lineno);
		ast->v.list = NULL;
		return ast;
	}

	/* Now we either have a regular parenthesized expression
	 * OR
	 * we have a non-empty tuple.
	 */

	AST *ast = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	peek = parser_peek_token(p);

	if (peek->type == TOK_COMMA) {
		/* we have a non-empty tuple */

		expect(p, TOK_COMMA);
		ERROR_CHECK_AST(p, NULL, ast);
		struct ast_list *list_head = ast_list_new();
		list_head->ast = ast;
		struct ast_list *list = list_head;

		do {
			Token *next = parser_peek_token(p);

			if (next->type == TOK_EOF) {
				parse_err_unclosed(p, paren_open);
				ERROR_CHECK_LIST(p, NULL, list_head);
			}

			if (next->type == TOK_PAREN_CLOSE) {
				break;
			}

			list->next = ast_list_new();
			list = list->next;
			list->ast = parse_expr_no_assign(p);
			ERROR_CHECK_LIST(p, list->ast, list_head);

			next = parser_peek_token(p);

			if (next->type == TOK_COMMA) {
				expect(p, TOK_COMMA);
				ERROR_CHECK_LIST(p, NULL, list_head);
			} else if (next->type != TOK_PAREN_CLOSE) {
				parse_err_unexpected_token(p, next);
				ERROR_CHECK_LIST(p, NULL, list_head);
			}
		} while (true);

		ast = ast_new(NODE_TUPLE, NULL, NULL, lineno);
		ast->v.list = list_head;
	}

	expect(p, TOK_PAREN_CLOSE);
	ERROR_CHECK_AST(p, NULL, ast);

	return ast;
}

static AST *parse_unop(Parser *p)
{
	Token *tok = parser_next_token(p);

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

	AST *atom = parse_atom(p);
	ERROR_CHECK(p);
	AST *ast = ast_new(type, atom, NULL, tok->lineno);
	return ast;
}

static AST *parse_int(Parser *p)
{
	Token *tok = expect(p, TOK_INT);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_INT, NULL, NULL, tok->lineno);
	ast->v.int_val = atoi(tok->value);
	return ast;
}

static AST *parse_float(Parser *p)
{
	Token *tok = expect(p, TOK_FLOAT);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_FLOAT, NULL, NULL, tok->lineno);
	ast->v.float_val = atof(tok->value);
	return ast;
}

static AST *parse_str(Parser *p)
{
	Token *tok = expect(p, TOK_STR);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_STRING, NULL, NULL, tok->lineno);

	// deal with quotes appropriately:
	ast->v.str_val = str_new_copy(tok->value + 1, tok->length - 2);

	return ast;
}

static AST *parse_ident(Parser *p)
{
	Token *tok = expect(p, TOK_IDENT);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_IDENT, NULL, NULL, tok->lineno);
	ast->v.ident = str_new_copy(tok->value, tok->length);
	return ast;
}

static AST *parse_dollar_ident(Parser *p)
{
	Token *tok = expect(p, TOK_DOLLAR);
	ERROR_CHECK(p);

	if (!p->in_lambda) {
		parse_err_misplaced_dollar_identifier(p, tok);
		return NULL;
	}

	const unsigned int value = atoi(&tok->value[1]);
	assert(value > 0);

	if (value > FUNCTION_MAX_PARAMS) {
		parse_err_too_many_params(p, tok);
		return NULL;
	}

	if (value > p->max_dollar_ident) {
		p->max_dollar_ident = value;
	}

	AST *ast = ast_new(NODE_IDENT, NULL, NULL, tok->lineno);
	ast->v.ident = str_new_copy(tok->value, tok->length);
	return ast;
}

static AST *parse_print(Parser *p)
{
	Token *tok = expect(p, TOK_PRINT);
	ERROR_CHECK(p);
	AST *expr = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_PRINT, expr, NULL, tok->lineno);
	return ast;
}

static AST *parse_if(Parser *p)
{
	Token *tok = expect(p, TOK_IF);
	ERROR_CHECK(p);
	AST *condition = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	AST *body = parse_block(p);
	ERROR_CHECK_AST(p, body, condition);
	AST *ast = ast_new(NODE_IF, condition, body, tok->lineno);
	ast->v.middle = NULL;

	AST *else_chain_base = NULL;
	AST *else_chain_last = NULL;

	while ((tok = parser_peek_token(p))->type == TOK_ELIF) {
		expect(p, TOK_ELIF);
		ERROR_CHECK_AST2(p, NULL, ast, else_chain_base);
		AST *elif_condition = parse_expr_no_assign(p);
		ERROR_CHECK_AST2(p, elif_condition, ast, else_chain_base);
		AST *elif_body = parse_block(p);
		ERROR_CHECK_AST3(p, elif_body, ast, else_chain_base, elif_condition);
		AST *elif = ast_new(NODE_ELIF, elif_condition, elif_body, tok->lineno);
		elif->v.middle = NULL;

		if (else_chain_base == NULL) {
			else_chain_base = else_chain_last = elif;
		} else {
			else_chain_last->v.middle = elif;
			else_chain_last = elif;
		}
	}

	if ((tok = parser_peek_token(p))->type == TOK_ELSE) {
		expect(p, TOK_ELSE);
		ERROR_CHECK_AST2(p, NULL, ast, else_chain_base);
		AST *else_body = parse_block(p);
		ERROR_CHECK_AST2(p, else_body, ast, else_chain_base);
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

static AST *parse_while(Parser *p)
{
	Token *tok = expect(p, TOK_WHILE);
	ERROR_CHECK(p);
	AST *condition = parse_expr_no_assign(p);
	ERROR_CHECK(p);

	const unsigned old_in_loop = p->in_loop;
	p->in_loop = 1;
	AST *body = parse_block(p);
	p->in_loop = old_in_loop;
	ERROR_CHECK_AST(p, body, condition);

	AST *ast = ast_new(NODE_WHILE, condition, body, tok->lineno);
	return ast;
}

static AST *parse_for(Parser *p)
{
	Token *tok = expect(p, TOK_FOR);
	ERROR_CHECK(p);

	AST *lcv = parse_ident(p);  // loop-control variable
	ERROR_CHECK(p);

	expect(p, TOK_IN);
	ERROR_CHECK_AST(p, NULL, lcv);

	AST *iter = parse_expr_no_assign(p);
	ERROR_CHECK_AST(p, iter, lcv);

	const unsigned old_in_loop = p->in_loop;
	p->in_loop = 1;
	AST *body = parse_block(p);
	p->in_loop = old_in_loop;
	ERROR_CHECK_AST2(p, body, lcv, iter);

	AST *ast = ast_new(NODE_FOR, lcv, iter, tok->lineno);
	ast->v.middle = body;
	return ast;
}

static AST *parse_def(Parser *p)
{
	Token *tok = expect(p, TOK_DEF);
	ERROR_CHECK(p);
	Token *name_tok = parser_peek_token(p);
	AST *name = parse_ident(p);
	ERROR_CHECK(p);

	unsigned int nargs;
	ParamList *params = parse_comma_separated_list(p,
	                                               TOK_PAREN_OPEN, TOK_PAREN_CLOSE,
	                                               parse_expr,
	                                               &nargs);
	ERROR_CHECK_AST(p, params, name);

	/* parameter syntax check */
	for (struct ast_list *param = params; param != NULL; param = param->next) {
		if (!((param->ast->type == NODE_ASSIGN && param->ast->left->type == NODE_IDENT) ||
		       param->ast->type == NODE_IDENT)) {
			parse_err_malformed_params(p, name_tok);
			ast_list_free(params);
			ast_free(name);
			return NULL;
		}
	}

	if (nargs > FUNCTION_MAX_PARAMS) {
		parse_err_too_many_params(p, name_tok);
		ast_list_free(params);
		ast_free(name);
		return NULL;
	}

	/* no non-default parameters after default ones */
	bool dflt = false;
	for (struct ast_list *param = params; param != NULL; param = param->next) {
		if (param->ast->type == NODE_ASSIGN) {
			dflt = true;
		} else if (dflt) {
			parse_err_non_default_after_default(p, name_tok);
			ast_list_free(params);
			ast_free(name);
			return NULL;
		}
	}

	/* no duplicate parameter names */
	for (struct ast_list *param = params; param != NULL; param = param->next) {
		AST *ast1 = (param->ast->type == NODE_ASSIGN) ? param->ast->left : param->ast;
		for (struct ast_list *check = params; check != param; check = check->next) {
			AST *ast2 = (check->ast->type == NODE_ASSIGN) ? check->ast->left : check->ast;
			if (str_eq(ast1->v.ident, ast2->v.ident)) {
				parse_err_dup_params(p, name_tok, ast1->v.ident->value);
				ast_free(name);
				ast_list_free(params);
				return NULL;
			}
		}
	}

	const unsigned old_in_function = p->in_function;
	const unsigned old_in_lambda = p->in_lambda;
	const unsigned old_in_loop = p->in_loop;
	p->in_function = 1;
	p->in_lambda = 0;
	p->in_loop = 0;
	AST *body = parse_block(p);
	p->in_function = old_in_function;
	p->in_lambda = old_in_lambda;
	p->in_loop = old_in_loop;

	if (PARSER_ERROR(p)) {
		assert(body == NULL);
		ast_free(name);
		ast_list_free(params);
		return NULL;
	}

	AST *ast = ast_new(NODE_DEF, name, body, tok->lineno);
	ast->v.params = params;
	return ast;
}

static AST *parse_break(Parser *p)
{
	Token *tok = expect(p, TOK_BREAK);
	ERROR_CHECK(p);

	if (!p->in_loop) {
		parse_err_invalid_break(p, tok);
		return NULL;
	}

	AST *ast = ast_new(NODE_BREAK, NULL, NULL, tok->lineno);
	return ast;
}

static AST *parse_continue(Parser *p)
{
	Token *tok = expect(p, TOK_CONTINUE);
	ERROR_CHECK(p);

	if (!p->in_loop) {
		parse_err_invalid_continue(p, tok);
		return NULL;
	}

	AST *ast = ast_new(NODE_CONTINUE, NULL, NULL, tok->lineno);
	return ast;
}

static AST *parse_return(Parser *p)
{
	Token *tok = expect(p, TOK_RETURN);
	ERROR_CHECK(p);

	if (!p->in_function) {
		parse_err_invalid_return(p, tok);
		return NULL;
	}

	AST *expr = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_RETURN, expr, NULL, tok->lineno);
	return ast;
}

static AST *parse_throw(Parser *p)
{
	Token *tok = expect(p, TOK_THROW);
	ERROR_CHECK(p);
	AST *expr = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_THROW, expr, NULL, tok->lineno);
	return ast;
}

static AST *parse_try_catch(Parser *p)
{
	Token *tok = expect(p, TOK_TRY);
	ERROR_CHECK(p);
	AST *try_body = parse_block(p);
	ERROR_CHECK(p);
	Token *catch = expect(p, TOK_CATCH);
	ERROR_CHECK_AST(p, catch, try_body);

	unsigned int count;
	struct ast_list *exc_list = parse_comma_separated_list(p,
	                                                       TOK_PAREN_OPEN,
	                                                       TOK_PAREN_CLOSE,
	                                                       parse_expr,
	                                                       &count);

	ERROR_CHECK_AST(p, exc_list, try_body);

	if (count == 0) {
		parse_err_empty_catch(p, catch);
		ast_free(try_body);
		ast_list_free(exc_list);
		return NULL;
	}

	AST *catch_body = parse_block(p);

	if (PARSER_ERROR(p)) {
		assert(catch_body == NULL);
		ast_free(try_body);
		ast_list_free(exc_list);
	}

	AST *ast = ast_new(NODE_TRY_CATCH, try_body, catch_body, tok->lineno);
	ast->v.excs = exc_list;
	return ast;
}

static AST *parse_import(Parser *p)
{
	Token *tok = expect(p, TOK_IMPORT);
	ERROR_CHECK(p);
	AST *ident = parse_ident(p);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_IMPORT, ident, NULL, tok->lineno);
	return ast;
}

static AST *parse_export(Parser *p)
{
	Token *tok = expect(p, TOK_EXPORT);
	ERROR_CHECK(p);
	AST *ident = parse_ident(p);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_EXPORT, ident, NULL, tok->lineno);
	return ast;
}

static AST *parse_block(Parser *p)
{
	Block *block_head = NULL;

	Token *peek = parser_peek_token(p);

	Token *brace_open;

	if (peek->type == TOK_COLON) {
		brace_open = expect(p, TOK_COLON);
		ERROR_CHECK(p);
		block_head = ast_list_new();
		block_head->ast = parse_stmt(p);
		ERROR_CHECK_LIST(p, block_head->ast, block_head);
	} else {
		brace_open = expect(p, TOK_BRACE_OPEN);
		ERROR_CHECK(p);
		Block *block = NULL;

		do {
			Token *next = parser_peek_token(p);

			if (next->type == TOK_EOF) {
				parse_err_unclosed(p, brace_open);
				return NULL;
			}

			if (next->type == TOK_BRACE_CLOSE) {
				break;
			}

			AST *stmt = parse_stmt(p);
			ERROR_CHECK_LIST(p, stmt, block_head);

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

		expect(p, TOK_BRACE_CLOSE);
		ERROR_CHECK_LIST(p, NULL, block_head);
	}

	AST *ast = ast_new(NODE_BLOCK, NULL, NULL, brace_open->lineno);
	ast->v.block = block_head;
	return ast;
}

static AST *parse_list(Parser *p)
{
	Token *brack_open = parser_peek_token(p);  /* parse_comma_separated_list does error checking */
	struct ast_list *list_head = parse_comma_separated_list(p,
	                                                        TOK_BRACK_OPEN,
	                                                        TOK_BRACK_CLOSE,
	                                                        parse_expr,
	                                                        NULL);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_LIST, NULL, NULL, brack_open->lineno);
	ast->v.list = list_head;
	return ast;
}

static AST *parse_lambda(Parser *p)
{
	Token *colon = parser_peek_token(p);
	expect(p, TOK_COLON);
	ERROR_CHECK(p);

	const unsigned old_max_dollar_ident = p->max_dollar_ident;
	const unsigned old_in_function = p->in_function;
	const unsigned old_in_lambda = p->in_lambda;
	const unsigned old_in_loop = p->in_loop;
	p->max_dollar_ident = 0;
	p->in_function = 1;
	p->in_loop = 0;
	p->in_lambda = 1;
	AST *body = parse_expr(p);
	const unsigned int max_dollar_ident = p->max_dollar_ident;
	p->max_dollar_ident = old_max_dollar_ident;
	p->in_function = old_in_function;
	p->in_lambda = old_in_lambda;
	p->in_loop = old_in_loop;

	ERROR_CHECK(p);

	AST *ast = ast_new(NODE_LAMBDA, body, NULL, colon->lineno);
	ast->v.max_dollar_ident = max_dollar_ident;
	return ast;
}

static AST *parse_empty(Parser *p)
{
	Token *tok = expect(p, TOK_SEMICOLON);
	ERROR_CHECK(p);
	AST *ast = ast_new(NODE_EMPTY, NULL, NULL, tok->lineno);
	return ast;
}

/*
 * Parses generic comma-separated list with given start and end delimiters.
 * Returns the number of elements in this parsed list.
 */
static struct ast_list *parse_comma_separated_list(Parser *p,
                                                   const TokType open_type, const TokType close_type,
                                                   AST *(*sub_element_parse_routine)(Parser *),
                                                   unsigned int *count)
{
	Token *tok_open = expect(p, open_type);
	ERROR_CHECK(p);
	struct ast_list *list_head = NULL;
	struct ast_list *list = NULL;

	unsigned int nelements = 0;

	do {
		Token *next = parser_peek_token(p);

		if (next->type == TOK_EOF) {
			parse_err_unclosed(p, tok_open);
			ERROR_CHECK_LIST(p, NULL, list_head);
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

		list->ast = sub_element_parse_routine(p);
		ERROR_CHECK_LIST(p, list->ast, list_head);
		++nelements;

		next = parser_peek_token(p);

		if (next->type == TOK_COMMA) {
			expect(p, TOK_COMMA);
			ERROR_CHECK_LIST(p, NULL, list_head);
		} else if (next->type != close_type) {
			parse_err_unexpected_token(p, next);
			ERROR_CHECK_LIST(p, NULL, list_head);
		}
	} while (true);

	expect(p, close_type);
	ERROR_CHECK_LIST(p, NULL, list_head);

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
	case TOK_AT:
		return NODE_APPLY;
	case TOK_DOT:
		return NODE_DOT;
	case TOK_DOTDOT:
		return NODE_DOTDOT;
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
	case TOK_ASSIGN_AT:
		return NODE_ASSIGN_APPLY;
	case TOK_IN:
		return NODE_IN;
	case TOK_IF:
		return NODE_COND_EXPR;
	default:
		INTERNAL_ERROR();
		return -1;
	}
}

static Token *expect(Parser *p, TokType type)
{
	assert(type != TOK_NONE);
	Token *next = parser_has_next_token(p) ? parser_next_token(p) : NULL;
	const TokType next_type = next ? next->type : TOK_NONE;

	if (next_type != type) {
		parse_err_unexpected_token(p, next);
		ERROR_CHECK(p);
	}

	return next;
}

/*
 * Parser error functions
 */

DECL_MIN_FUNC(min, size_t)

static const char *err_on_tok(Parser *p, Token *tok)
{
	return err_on_char(tok->value, p->code, p->end, tok->lineno);
}

static void parse_err_unexpected_token(Parser *p, Token *tok)
{
#define MAX_LEN 1024

	if (tok->type == TOK_EOF) {
		/*
		 * This should really always be true,
		 * since we shouldn't have an unexpected
		 * token in an empty file.
		 */
		if (p->tok_count > 1) {
			const char *tok_err = "";
			bool free_tok_err = false;
			Token *tokens = p->tokens;
			size_t last = p->tok_count - 2;

			while (last > 0 && tokens[last].type == TOK_NEWLINE) {
				--last;
			}

			if (tokens[last].type != TOK_NEWLINE) {
				tok_err = err_on_tok(p, &tokens[last]);
				free_tok_err = true;
			}

			PARSER_SET_ERROR_MSG(p,
			                     str_format(SYNTAX_ERROR " unexpected end-of-file after token\n\n%s",
			                                p->name, tok->lineno, tok_err));

			if (free_tok_err) {
				FREE(tok_err);
			}
		} else {
			PARSER_SET_ERROR_MSG(p,
			                     str_format(SYNTAX_ERROR " unexpected end-of-file after token\n\n",
			                                p->name, tok->lineno));
		}
	} else {
		char tok_str[MAX_LEN];

		const size_t tok_len = min(tok->length, MAX_LEN - 1);
		memcpy(tok_str, tok->value, tok_len);
		tok_str[tok_len] = '\0';

		const char *tok_err = err_on_tok(p, tok);
		PARSER_SET_ERROR_MSG(p,
		                     str_format(SYNTAX_ERROR " unexpected token: %s\n\n%s",
		                                p->name, tok->lineno, tok_str, tok_err));
		FREE(tok_err);
	}

	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_UNEXPECTED_TOKEN);

#undef MAX_LEN
}

static void parse_err_not_a_statement(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " not a statement\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_NOT_A_STATEMENT);
}

static void parse_err_unclosed(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " unclosed\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_UNCLOSED);
}

static void parse_err_invalid_assign(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " misplaced assignment\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_INVALID_ASSIGN);
}

static void parse_err_invalid_break(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " misplaced break statement\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_INVALID_BREAK);
}

static void parse_err_invalid_continue(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " misplaced continue statement\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_INVALID_CONTINUE);
}

static void parse_err_invalid_return(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " misplaced return statement\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_INVALID_RETURN);
}

static void parse_err_too_many_params(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " function has too many parameters (max %d)\n\n%s",
	                                p->name, tok->lineno, FUNCTION_MAX_PARAMS, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_TOO_MANY_PARAMETERS);
}

static void parse_err_dup_params(Parser *p, Token *tok, const char *param)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " function has duplicate parameter '%s'\n\n%s",
	                                p->name, tok->lineno, param, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_DUPLICATE_PARAMETERS);
}

static void parse_err_non_default_after_default(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " non-default parameter after default parameter\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_NON_DEFAULT_AFTER_DEFAULT_PARAMETERS);
}

static void parse_err_malformed_params(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " function has malformed parameters\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_MALFORMED_PARAMETERS);
}

static void parse_err_too_many_args(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " function call has too many arguments (max %d)\n\n%s",
	                                p->name, tok->lineno, FUNCTION_MAX_PARAMS, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_TOO_MANY_ARGUMENTS);
}

static void parse_err_dup_named_args(Parser *p, Token *tok, const char *name)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " function call has duplicate named argument '%s'\n\n%s",
	                                p->name, tok->lineno, name, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_DUPLICATE_NAMED_ARGUMENTS);
}

static void parse_err_unnamed_after_named(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " unnamed arguments after named arguments\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_UNNAMED_AFTER_NAMED_ARGUMENTS);
}

static void parse_err_malformed_args(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " function call has malformed arguments\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_MALFORMED_ARGUMENTS);
}

static void parse_err_empty_catch(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " empty catch statement\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_EMPTY_CATCH);
}

static void parse_err_misplaced_dollar_identifier(Parser *p, Token *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " dollar identifier outside lambda\n\n%s",
	                                p->name, tok->lineno, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_MISPLACED_DOLLAR_IDENTIFIER);
}
