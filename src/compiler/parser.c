#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "str.h"
#include "err.h"
#include "util.h"
#include "lexer.h"
#include "parser.h"

#define ERROR_CHECK(p) do { if (RHO_PARSER_ERROR(p)) return NULL; } while (0)

#define ERROR_CHECK_AST(p, should_be_null, free_me) \
	do { \
		if (RHO_PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			rho_ast_free(free_me); \
			return NULL; \
		} \
	} while (0)

#define ERROR_CHECK_AST2(p, should_be_null, free_me1, free_me2) \
	do { \
		if (RHO_PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			rho_ast_free(free_me1); \
			rho_ast_free(free_me2); \
			return NULL; \
		} \
	} while (0)

#define ERROR_CHECK_AST3(p, should_be_null, free_me1, free_me2, free_me3) \
	do { \
		if (RHO_PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			rho_ast_free(free_me1); \
			rho_ast_free(free_me2); \
			rho_ast_free(free_me3); \
			return NULL; \
		} \
	} while (0)

#define ERROR_CHECK_LIST(p, should_be_null, free_me) \
	do { \
		if (RHO_PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			rho_ast_list_free(free_me); \
			return NULL; \
		} \
	} while (0)

typedef struct {
	RhoTokType type;
	unsigned int prec;  // precedence of operator
	bool assoc;         // associativity: true = left, false = right
} Op;

static Op op_from_tok_type(RhoTokType type);
static RhoNodeType nodetype_from_op(Op op);

static const Op ops[] = {
	/*
	OP,                 PREC,        ASSOC */
	{RHO_TOK_PLUS,          70,          true},
	{RHO_TOK_MINUS,         70,          true},
	{RHO_TOK_MUL,           80,          true},
	{RHO_TOK_DIV,           80,          true},
	{RHO_TOK_MOD,           80,          true},
	{RHO_TOK_POW,           90,          false},
	{RHO_TOK_BITAND,        32,          true},
	{RHO_TOK_BITOR,         30,          true},
	{RHO_TOK_XOR,           31,          true},
	{RHO_TOK_SHIFTL,        60,          true},
	{RHO_TOK_SHIFTR,        60,          true},
	{RHO_TOK_AND,           21,          true},
	{RHO_TOK_OR,            20,          true},
	{RHO_TOK_EQUAL,         40,          true},
	{RHO_TOK_NOTEQ,         40,          true},
	{RHO_TOK_LT,            50,          true},
	{RHO_TOK_GT,            50,          true},
	{RHO_TOK_LE,            50,          true},
	{RHO_TOK_GE,            50,          true},
	{RHO_TOK_ASSIGN,        10,          true},
	{RHO_TOK_ASSIGN_ADD,    10,          true},
	{RHO_TOK_ASSIGN_SUB,    10,          true},
	{RHO_TOK_ASSIGN_MUL,    10,          true},
	{RHO_TOK_ASSIGN_DIV,    10,          true},
	{RHO_TOK_ASSIGN_MOD,    10,          true},
	{RHO_TOK_ASSIGN_POW,    10,          true},
	{RHO_TOK_ASSIGN_BITAND, 10,          true},
	{RHO_TOK_ASSIGN_BITOR,  10,          true},
	{RHO_TOK_ASSIGN_XOR,    10,          true},
	{RHO_TOK_ASSIGN_SHIFTL, 10,          true},
	{RHO_TOK_ASSIGN_SHIFTR, 10,          true},
	{RHO_TOK_ASSIGN_AT,     10,          true},
	{RHO_TOK_DOT,           99,          true},
	{RHO_TOK_DOTDOT,        92,          true},
	{RHO_TOK_AT,            91,          false},
	{RHO_TOK_IN,             9,          true},
	{RHO_TOK_IF,            22,          true},  /* ternary operator */
};

static const size_t ops_size = (sizeof(ops) / sizeof(Op));

#define FUNCTION_MAX_PARAMS 128

static RhoAST *parse_stmt(RhoParser *p);

static RhoAST *parse_expr(RhoParser *p);
static RhoAST *parse_expr_no_assign(RhoParser *p);
static RhoAST *parse_parens(RhoParser *p);
static RhoAST *parse_atom(RhoParser *p);
static RhoAST *parse_unop(RhoParser *p);

static RhoAST *parse_null(RhoParser *p);
static RhoAST *parse_int(RhoParser *p);
static RhoAST *parse_float(RhoParser *p);
static RhoAST *parse_str(RhoParser *p);
static RhoAST *parse_ident(RhoParser *p);
static RhoAST *parse_dollar_ident(RhoParser *p);

static RhoAST *parse_print(RhoParser *p);
static RhoAST *parse_if(RhoParser *p);
static RhoAST *parse_while(RhoParser *p);
static RhoAST *parse_for(RhoParser *p);
static RhoAST *parse_def(RhoParser *p);
static RhoAST *parse_gen(RhoParser *p);
static RhoAST *parse_act(RhoParser *p);
static RhoAST *parse_break(RhoParser *p);
static RhoAST *parse_continue(RhoParser *p);
static RhoAST *parse_return(RhoParser *p);
static RhoAST *parse_throw(RhoParser *p);
static RhoAST *parse_produce(RhoParser *p);
static RhoAST *parse_receive(RhoParser *p);
static RhoAST *parse_try_catch(RhoParser *p);
static RhoAST *parse_import(RhoParser *p);
static RhoAST *parse_export(RhoParser *p);

static RhoAST *parse_block(RhoParser *p);
static RhoAST *parse_list(RhoParser *p);
static RhoAST *parse_set_or_dict(RhoParser *p);
static RhoAST *parse_lambda(RhoParser *p);

static RhoAST *parse_empty(RhoParser *p);

static struct rho_ast_list *parse_comma_separated_list(RhoParser *p,
                                                       const RhoTokType open_type, const RhoTokType close_type,
                                                       RhoAST *(*sub_element_parse_routine)(RhoParser *),
                                                       unsigned int *count);

static RhoToken *expect(RhoParser *p, RhoTokType type);

static void parse_err_unexpected_token(RhoParser *p, RhoToken *tok);
static void parse_err_not_a_statement(RhoParser *p, RhoToken *tok);
static void parse_err_unclosed(RhoParser *p, RhoToken *tok);
static void parse_err_invalid_assign(RhoParser *p, RhoToken *tok);
static void parse_err_invalid_break(RhoParser *p, RhoToken *tok);
static void parse_err_invalid_continue(RhoParser *p, RhoToken *tok);
static void parse_err_invalid_return(RhoParser *p, RhoToken *tok);
static void parse_err_invalid_produce(RhoParser *p, RhoToken *tok);
static void parse_err_invalid_receive(RhoParser *p, RhoToken *tok);
static void parse_err_too_many_params(RhoParser *p, RhoToken *tok);
static void parse_err_dup_params(RhoParser *p, RhoToken *tok, const char *param);
static void parse_err_non_default_after_default(RhoParser *p, RhoToken *tok);
static void parse_err_malformed_params(RhoParser *p, RhoToken *tok);
static void parse_err_too_many_args(RhoParser *p, RhoToken *tok);
static void parse_err_dup_named_args(RhoParser *p, RhoToken *tok, const char *name);
static void parse_err_unnamed_after_named(RhoParser *p, RhoToken *tok);
static void parse_err_malformed_args(RhoParser *p, RhoToken *tok);
static void parse_err_empty_catch(RhoParser *p, RhoToken *tok);
static void parse_err_misplaced_dollar_identifier(RhoParser *p, RhoToken *tok);
static void parse_err_inconsistent_dict_elements(RhoParser *p, RhoToken *tok);
static void parse_err_empty_for_params(RhoParser *p, RhoToken *tok);
static void parse_err_return_val_in_gen(RhoParser *p, RhoToken *tok);

RhoParser *rho_parser_new(char *str, const char *name)
{
#define INITIAL_TOKEN_ARRAY_CAPACITY 5
	RhoParser *p = rho_malloc(sizeof(RhoParser));
	p->code = str;
	p->end = &str[strlen(str) - 1];
	p->pos = &str[0];
	p->mark = 0;
	p->tokens = rho_malloc(INITIAL_TOKEN_ARRAY_CAPACITY * sizeof(RhoToken));
	p->tok_count = 0;
	p->tok_capacity = INITIAL_TOKEN_ARRAY_CAPACITY;
	p->tok_pos = 0;
	p->lineno = 1;
	p->peek = NULL;
	p->name = name;
	p->in_function = 0;
	p->in_lambda = 0;
	p->in_generator = 0;
	p->in_actor = 0;
	p->in_loop = 0;
	p->error_type = RHO_PARSE_ERR_NONE;
	p->error_msg = NULL;

	rho_parser_tokenize(p);

	return p;
#undef INITIAL_TOKEN_ARRAY_CAPACITY
}

void rho_parser_free(RhoParser *p)
{
	free(p->tokens);
	RHO_FREE(p->error_msg);
	free(p);
}

RhoProgram *rho_parse(RhoParser *p)
{
	RhoProgram *head = rho_ast_list_new();
	struct rho_ast_list *node = head;

	while (rho_parser_has_next_token(p)) {
		RhoAST *stmt = parse_stmt(p);
		ERROR_CHECK_LIST(p, stmt, head);

		if (stmt == NULL) {
			break;
		}

		/*
		 * We don't include empty statements
		 * in the syntax tree.
		 */
		if (stmt->type == RHO_NODE_EMPTY) {
			free(stmt);
			continue;
		}

		if (node->ast != NULL) {
			node->next = rho_ast_list_new();
			node = node->next;
		}

		node->ast = stmt;
	}

	return head;
}

/*
 * Parses a top-level statement
 */
static RhoAST *parse_stmt(RhoParser *p)
{
	RhoToken *tok = rho_parser_peek_token(p);

	RhoAST *stmt;

	switch (tok->type) {
	case RHO_TOK_PRINT:
		stmt = parse_print(p);
		break;
	case RHO_TOK_IF:
		stmt = parse_if(p);
		break;
	case RHO_TOK_WHILE:
		stmt = parse_while(p);
		break;
	case RHO_TOK_FOR:
		stmt = parse_for(p);
		break;
	case RHO_TOK_DEF:
		stmt = parse_def(p);
		break;
	case RHO_TOK_GEN:
		stmt = parse_gen(p);
		break;
	case RHO_TOK_ACT:
		stmt = parse_act(p);
		break;
	case RHO_TOK_BREAK:
		stmt = parse_break(p);
		break;
	case RHO_TOK_CONTINUE:
		stmt = parse_continue(p);
		break;
	case RHO_TOK_RETURN:
		stmt = parse_return(p);
		break;
	case RHO_TOK_THROW:
		stmt = parse_throw(p);
		break;
	case RHO_TOK_PRODUCE:
		stmt = parse_produce(p);
		break;
	case RHO_TOK_RECEIVE:
		stmt = parse_receive(p);
		break;
	case RHO_TOK_TRY:
		stmt = parse_try_catch(p);
		break;
	case RHO_TOK_IMPORT:
		stmt = parse_import(p);
		break;
	case RHO_TOK_EXPORT:
		stmt = parse_export(p);
		break;
	case RHO_TOK_SEMICOLON:
		return parse_empty(p);
	case RHO_TOK_EOF:
		return NULL;
	default: {
		RhoAST *expr_stmt = parse_expr(p);
		ERROR_CHECK(p);
		const RhoNodeType type = expr_stmt->type;

		/*
		 * Not every expression is considered a statement. For
		 * example, the expression "2 + 2" on its own does not
		 * have a useful effect and is therefore not considered
		 * a valid statement. An assignment like "a = 2", on the
		 * other hand, is considered a valid statement. We must
		 * ensure that the expression we have just parsed is a
		 * valid statement.
		 */
		if (!RHO_NODE_TYPE_IS_EXPR_STMT(type)) {
			parse_err_not_a_statement(p, tok);
			rho_ast_free(expr_stmt);
			return NULL;
		}

		stmt = expr_stmt;
	}
	}
	ERROR_CHECK(p);

	RhoToken *stmt_end = rho_parser_peek_token_direct(p);
	const RhoTokType stmt_end_type = stmt_end->type;

	if (!RHO_TOK_TYPE_IS_STMT_TERM(stmt_end_type)) {
		parse_err_unexpected_token(p, stmt_end);
		rho_ast_free(stmt);
		return NULL;
	}

	return stmt;
}

static RhoAST *parse_expr_helper(RhoParser *p, const bool allow_assigns);

static RhoAST *parse_expr_min_prec(RhoParser *p, unsigned int min_prec, bool allow_assigns);

static RhoAST *parse_expr(RhoParser *p)
{
	return parse_expr_helper(p, true);
}

static RhoAST *parse_expr_no_assign(RhoParser *p)
{
	return parse_expr_helper(p, false);
}

static RhoAST *parse_expr_helper(RhoParser *p, const bool allow_assigns)
{
	return parse_expr_min_prec(p, 1, allow_assigns);
}

/*
 * Implementation of precedence climbing method.
 */
static RhoAST *parse_expr_min_prec(RhoParser *p, unsigned int min_prec, bool allow_assigns)
{
	RhoAST *lhs = parse_atom(p);
	ERROR_CHECK(p);

	while (rho_parser_has_next_token(p)) {

		RhoToken *tok = rho_parser_peek_token(p);
		const RhoTokType type = tok->type;
		RhoToken *next_direct = rho_parser_peek_token_direct(p);

		/*
		 * The 2nd component of the if-statement below is needed to
		 * distinguish something like:
		 *
		 *     print x if c else ...
		 *
		 * from, say:
		 *
		 *     print x
		 *     if c { ... }
		 */
		if (!RHO_TOK_TYPE_IS_OP(type) && !(type == RHO_TOK_IF && (tok == next_direct))) {
			break;
		}

		const Op op = op_from_tok_type(type);

		if (op.prec < min_prec) {
			break;
		}

		if (RHO_TOK_TYPE_IS_ASSIGNMENT_TOK(op.type) &&
		    (!allow_assigns || min_prec != 1 || !RHO_NODE_TYPE_IS_ASSIGNABLE(lhs->type))) {
			parse_err_invalid_assign(p, tok);
			rho_ast_free(lhs);
			return NULL;
		}

		const unsigned int next_min_prec = op.assoc ? (op.prec + 1) : op.prec;

		rho_parser_next_token(p);

		const bool ternary = (op.type == RHO_TOK_IF);
		RhoAST *cond = NULL;

		if (ternary) {  /* ternary operator */
			cond = parse_expr_no_assign(p);
			expect(p, RHO_TOK_ELSE);
			ERROR_CHECK_AST2(p, NULL, cond, lhs);
		}

		RhoAST *rhs = parse_expr_min_prec(p, next_min_prec, false);
		ERROR_CHECK_AST2(p, rhs, lhs, cond);

		RhoNodeType node_type = nodetype_from_op(op);
		RhoAST *ast = rho_ast_new(node_type, lhs, rhs, tok->lineno);

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
static RhoAST *parse_atom(RhoParser *p)
{
	RhoToken *tok = rho_parser_peek_token(p);
	RhoAST *ast;

	switch (tok->type) {
	case RHO_TOK_PAREN_OPEN:
		ast = parse_parens(p);
		break;
	case RHO_TOK_NULL:
		ast = parse_null(p);
		break;
	case RHO_TOK_INT:
		ast = parse_int(p);
		break;
	case RHO_TOK_FLOAT:
		ast = parse_float(p);
		break;
	case RHO_TOK_STR:
		ast = parse_str(p);
		break;
	case RHO_TOK_IDENT:
		ast = parse_ident(p);
		break;
	case RHO_TOK_DOLLAR:
		ast = parse_dollar_ident(p);
		break;
	case RHO_TOK_BRACK_OPEN:
		ast = parse_list(p);
		break;
	case RHO_TOK_BRACE_OPEN:
		ast = parse_set_or_dict(p);
		break;
	case RHO_TOK_NOT:
	case RHO_TOK_BITNOT:
	case RHO_TOK_PLUS:
	case RHO_TOK_MINUS:
		ast = parse_unop(p);
		break;
	case RHO_TOK_COLON:
		ast = parse_lambda(p);
		break;
	default:
		parse_err_unexpected_token(p, tok);
		ast = NULL;
		return NULL;
	}

	ERROR_CHECK(p);

	if (!rho_parser_has_next_token(p)) {
		goto end;
	}

	tok = rho_parser_peek_token(p);

	/*
	 * Deal with cases like `foo[7].bar(42)`...
	 */
	while (tok->type == RHO_TOK_DOT || tok->type == RHO_TOK_PAREN_OPEN || tok->type == RHO_TOK_BRACK_OPEN) {
		switch (tok->type) {
		case RHO_TOK_DOT: {
			RhoToken *dot_tok = expect(p, RHO_TOK_DOT);
			ERROR_CHECK_AST(p, dot_tok, ast);
			RhoAST *ident = parse_ident(p);
			ERROR_CHECK_AST(p, ident, ast);
			RhoAST *dot = rho_ast_new(RHO_NODE_DOT, ast, ident, dot_tok->lineno);
			ast = dot;
			break;
		}
		case RHO_TOK_PAREN_OPEN: {
			unsigned int nargs;
			RhoParamList *params = parse_comma_separated_list(p,
			                                                  RHO_TOK_PAREN_OPEN, RHO_TOK_PAREN_CLOSE,
			                                                  parse_expr,
			                                                  &nargs);

			ERROR_CHECK_AST(p, params, ast);

			/* argument syntax check */
			for (struct rho_ast_list *param = params; param != NULL; param = param->next) {
				if (RHO_NODE_TYPE_IS_ASSIGNMENT(param->ast->type) &&
				      (param->ast->type != RHO_NODE_ASSIGN ||
				       param->ast->left->type != RHO_NODE_IDENT)) {
					parse_err_malformed_args(p, tok);
					rho_ast_list_free(params);
					rho_ast_free(ast);
					return NULL;
				}
			}

			if (nargs > FUNCTION_MAX_PARAMS) {
				parse_err_too_many_args(p, tok);
				rho_ast_list_free(params);
				rho_ast_free(ast);
				return NULL;
			}

			/* no unnamed arguments after named ones */
			bool named = false;
			for (struct rho_ast_list *param = params; param != NULL; param = param->next) {
				if (param->ast->type == RHO_NODE_ASSIGN) {
					named = true;
				} else if (named) {
					parse_err_unnamed_after_named(p, tok);
					rho_ast_list_free(params);
					rho_ast_free(ast);
					return NULL;
				}
			}

			/* no duplicate named arguments */
			for (struct rho_ast_list *param = params; param != NULL; param = param->next) {
				if (param->ast->type != RHO_NODE_ASSIGN) {
					continue;
				}
				RhoAST *ast1 = param->ast->left;

				for (struct rho_ast_list *check = params; check != param; check = check->next) {
					if (check->ast->type != RHO_NODE_ASSIGN) {
						continue;
					}
					RhoAST *ast2 = check->ast->left;

					if (rho_str_eq(ast1->v.ident, ast2->v.ident)) {
						parse_err_dup_named_args(p, tok, ast1->v.ident->value);
						rho_ast_list_free(params);
						rho_ast_free(ast);
						return NULL;
					}
				}
			}

			RhoAST *call = rho_ast_new(RHO_NODE_CALL, ast, NULL, tok->lineno);
			call->v.params = params;
			ast = call;
			break;
		}
		case RHO_TOK_BRACK_OPEN: {
			expect(p, RHO_TOK_BRACK_OPEN);
			ERROR_CHECK_AST(p, NULL, ast);
			RhoAST *index = parse_expr_no_assign(p);
			ERROR_CHECK_AST(p, index, ast);
			expect(p, RHO_TOK_BRACK_CLOSE);
			ERROR_CHECK_AST2(p, NULL, ast, index);
			RhoAST *index_expr = rho_ast_new(RHO_NODE_INDEX, ast, index, tok->lineno);
			ast = index_expr;
			break;
		}
		default: {
			RHO_INTERNAL_ERROR();
		}
		}

		tok = rho_parser_peek_token(p);
	}

	end:
	return ast;
}

/*
 * Parses parenthesized expression.
 */
static RhoAST *parse_parens(RhoParser *p)
{
	RhoToken *paren_open = expect(p, RHO_TOK_PAREN_OPEN);
	ERROR_CHECK(p);
	const unsigned int lineno = paren_open->lineno;

	RhoToken *peek = rho_parser_peek_token(p);

	if (peek->type == RHO_TOK_PAREN_CLOSE) {
		/* we have an empty tuple */

		expect(p, RHO_TOK_PAREN_CLOSE);
		ERROR_CHECK(p);
		RhoAST *ast = rho_ast_new(RHO_NODE_TUPLE, NULL, NULL, lineno);
		ast->v.list = NULL;
		return ast;
	}

	/* Now we either have a regular parenthesized expression
	 * OR
	 * we have a non-empty tuple.
	 */

	RhoAST *ast = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	peek = rho_parser_peek_token(p);

	if (peek->type == RHO_TOK_COMMA) {
		/* we have a non-empty tuple */

		expect(p, RHO_TOK_COMMA);
		ERROR_CHECK_AST(p, NULL, ast);
		struct rho_ast_list *list_head = rho_ast_list_new();
		list_head->ast = ast;
		struct rho_ast_list *list = list_head;

		do {
			RhoToken *next = rho_parser_peek_token(p);

			if (next->type == RHO_TOK_EOF) {
				parse_err_unclosed(p, paren_open);
				ERROR_CHECK_LIST(p, NULL, list_head);
			}

			if (next->type == RHO_TOK_PAREN_CLOSE) {
				break;
			}

			list->next = rho_ast_list_new();
			list = list->next;
			list->ast = parse_expr_no_assign(p);
			ERROR_CHECK_LIST(p, list->ast, list_head);

			next = rho_parser_peek_token(p);

			if (next->type == RHO_TOK_COMMA) {
				expect(p, RHO_TOK_COMMA);
				ERROR_CHECK_LIST(p, NULL, list_head);
			} else if (next->type != RHO_TOK_PAREN_CLOSE) {
				parse_err_unexpected_token(p, next);
				ERROR_CHECK_LIST(p, NULL, list_head);
			}
		} while (true);

		ast = rho_ast_new(RHO_NODE_TUPLE, NULL, NULL, lineno);
		ast->v.list = list_head;
	}

	expect(p, RHO_TOK_PAREN_CLOSE);
	ERROR_CHECK_AST(p, NULL, ast);

	return ast;
}

static RhoAST *parse_unop(RhoParser *p)
{
	RhoToken *tok = rho_parser_next_token(p);

	RhoNodeType type;
	switch (tok->type) {
	case RHO_TOK_PLUS:
		type = RHO_NODE_UPLUS;
		break;
	case RHO_TOK_MINUS:
		type = RHO_NODE_UMINUS;
		break;
	case RHO_TOK_BITNOT:
		type = RHO_NODE_BITNOT;
		break;
	case RHO_TOK_NOT:
		type = RHO_NODE_NOT;
		break;
	default:
		type = RHO_NODE_EMPTY;
		RHO_INTERNAL_ERROR();
		break;
	}

	RhoAST *atom = parse_atom(p);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(type, atom, NULL, tok->lineno);
	return ast;
}

static RhoAST *parse_null(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_NULL);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_NULL, NULL, NULL, tok->lineno);
	return ast;
}

static RhoAST *parse_int(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_INT);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_INT, NULL, NULL, tok->lineno);
	ast->v.int_val = atoi(tok->value);
	return ast;
}

static RhoAST *parse_float(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_FLOAT);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_FLOAT, NULL, NULL, tok->lineno);
	ast->v.float_val = atof(tok->value);
	return ast;
}

static RhoAST *parse_str(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_STR);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_STRING, NULL, NULL, tok->lineno);

	// deal with quotes appropriately:
	ast->v.str_val = rho_str_new_copy(tok->value + 1, tok->length - 2);

	return ast;
}

static RhoAST *parse_ident(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_IDENT);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_IDENT, NULL, NULL, tok->lineno);
	ast->v.ident = rho_str_new_copy(tok->value, tok->length);
	return ast;
}

static RhoAST *parse_dollar_ident(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_DOLLAR);
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

	RhoAST *ast = rho_ast_new(RHO_NODE_IDENT, NULL, NULL, tok->lineno);
	ast->v.ident = rho_str_new_copy(tok->value, tok->length);
	return ast;
}

static RhoAST *parse_print(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_PRINT);
	ERROR_CHECK(p);
	RhoAST *expr = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_PRINT, expr, NULL, tok->lineno);
	return ast;
}

static RhoAST *parse_if(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_IF);
	ERROR_CHECK(p);
	RhoAST *condition = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	RhoAST *body = parse_block(p);
	ERROR_CHECK_AST(p, body, condition);
	RhoAST *ast = rho_ast_new(RHO_NODE_IF, condition, body, tok->lineno);
	ast->v.middle = NULL;

	RhoAST *else_chain_base = NULL;
	RhoAST *else_chain_last = NULL;

	while ((tok = rho_parser_peek_token(p))->type == RHO_TOK_ELIF) {
		expect(p, RHO_TOK_ELIF);
		ERROR_CHECK_AST2(p, NULL, ast, else_chain_base);
		RhoAST *elif_condition = parse_expr_no_assign(p);
		ERROR_CHECK_AST2(p, elif_condition, ast, else_chain_base);
		RhoAST *elif_body = parse_block(p);
		ERROR_CHECK_AST3(p, elif_body, ast, else_chain_base, elif_condition);
		RhoAST *elif = rho_ast_new(RHO_NODE_ELIF, elif_condition, elif_body, tok->lineno);
		elif->v.middle = NULL;

		if (else_chain_base == NULL) {
			else_chain_base = else_chain_last = elif;
		} else {
			else_chain_last->v.middle = elif;
			else_chain_last = elif;
		}
	}

	if ((tok = rho_parser_peek_token(p))->type == RHO_TOK_ELSE) {
		expect(p, RHO_TOK_ELSE);
		ERROR_CHECK_AST2(p, NULL, ast, else_chain_base);
		RhoAST *else_body = parse_block(p);
		ERROR_CHECK_AST2(p, else_body, ast, else_chain_base);
		RhoAST *else_ast = rho_ast_new(RHO_NODE_ELSE, else_body, NULL, tok->lineno);

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

static RhoAST *parse_while(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_WHILE);
	ERROR_CHECK(p);
	RhoAST *condition = parse_expr_no_assign(p);
	ERROR_CHECK(p);

	const unsigned old_in_loop = p->in_loop;
	p->in_loop = 1;
	RhoAST *body = parse_block(p);
	p->in_loop = old_in_loop;
	ERROR_CHECK_AST(p, body, condition);

	RhoAST *ast = rho_ast_new(RHO_NODE_WHILE, condition, body, tok->lineno);
	return ast;
}

static RhoAST *parse_for(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_FOR);
	ERROR_CHECK(p);

	RhoToken *peek = rho_parser_peek_token(p);
	RhoAST *lcv;

	if (peek->type == RHO_TOK_PAREN_OPEN) {
		unsigned int count;
		struct rho_ast_list *vars = parse_comma_separated_list(p,
		                                                       RHO_TOK_PAREN_OPEN, RHO_TOK_PAREN_CLOSE,
		                                                       parse_ident, &count);
		ERROR_CHECK_LIST(p, NULL, vars);

		if (vars == NULL) {
			parse_err_empty_for_params(p, peek);
			ERROR_CHECK(p);
		}

		lcv = rho_ast_new(RHO_NODE_TUPLE, NULL, NULL, peek->lineno);
		lcv->v.list = vars;
	} else {
		lcv = parse_ident(p);  // loop-control variable
	}

	ERROR_CHECK(p);

	expect(p, RHO_TOK_IN);
	ERROR_CHECK_AST(p, NULL, lcv);

	RhoAST *iter = parse_expr_no_assign(p);
	ERROR_CHECK_AST(p, iter, lcv);

	const unsigned old_in_loop = p->in_loop;
	p->in_loop = 1;
	RhoAST *body = parse_block(p);
	p->in_loop = old_in_loop;
	ERROR_CHECK_AST2(p, body, lcv, iter);

	RhoAST *ast = rho_ast_new(RHO_NODE_FOR, lcv, iter, tok->lineno);
	ast->v.middle = body;
	return ast;
}

#define PARSE_DEF 0
#define PARSE_GEN 1
#define PARSE_ACT 2

static RhoAST *parse_def_or_gen_or_act(RhoParser *p, const int select)
{
	RhoToken *tok;

	switch (select) {
	case PARSE_DEF:
		tok = expect(p, RHO_TOK_DEF);
		break;
	case PARSE_GEN:
		tok = expect(p, RHO_TOK_GEN);
		break;
	case PARSE_ACT:
		tok = expect(p, RHO_TOK_ACT);
		break;
	default:
		RHO_INTERNAL_ERROR();
	}

	ERROR_CHECK(p);
	RhoToken *name_tok = rho_parser_peek_token(p);
	RhoAST *name = parse_ident(p);
	ERROR_CHECK(p);

	unsigned int nargs;
	RhoParamList *params = parse_comma_separated_list(p,
	                                                  RHO_TOK_PAREN_OPEN, RHO_TOK_PAREN_CLOSE,
	                                                  parse_expr,
	                                                  &nargs);
	ERROR_CHECK_AST(p, params, name);

	/* parameter syntax check */
	for (struct rho_ast_list *param = params; param != NULL; param = param->next) {
		if (!((param->ast->type == RHO_NODE_ASSIGN && param->ast->left->type == RHO_NODE_IDENT) ||
		       param->ast->type == RHO_NODE_IDENT)) {
			parse_err_malformed_params(p, name_tok);
			rho_ast_list_free(params);
			rho_ast_free(name);
			return NULL;
		}
	}

	if (nargs > FUNCTION_MAX_PARAMS) {
		parse_err_too_many_params(p, name_tok);
		rho_ast_list_free(params);
		rho_ast_free(name);
		return NULL;
	}

	/* no non-default parameters after default ones */
	bool dflt = false;
	for (struct rho_ast_list *param = params; param != NULL; param = param->next) {
		if (param->ast->type == RHO_NODE_ASSIGN) {
			dflt = true;
		} else if (dflt) {
			parse_err_non_default_after_default(p, name_tok);
			rho_ast_list_free(params);
			rho_ast_free(name);
			return NULL;
		}
	}

	/* no duplicate parameter names */
	for (struct rho_ast_list *param = params; param != NULL; param = param->next) {
		RhoAST *ast1 = (param->ast->type == RHO_NODE_ASSIGN) ? param->ast->left : param->ast;
		for (struct rho_ast_list *check = params; check != param; check = check->next) {
			RhoAST *ast2 = (check->ast->type == RHO_NODE_ASSIGN) ? check->ast->left : check->ast;
			if (rho_str_eq(ast1->v.ident, ast2->v.ident)) {
				parse_err_dup_params(p, name_tok, ast1->v.ident->value);
				rho_ast_free(name);
				rho_ast_list_free(params);
				return NULL;
			}
		}
	}

	const unsigned old_in_function = p->in_function;
	const unsigned old_in_generator = p->in_generator;
	const unsigned old_in_actor = p->in_actor;
	const unsigned old_in_lambda = p->in_lambda;
	const unsigned old_in_loop = p->in_loop;

	switch (select) {
	case PARSE_DEF:
		p->in_function = 1;
		p->in_generator = 0;
		p->in_actor = 0;
		break;
	case PARSE_GEN:
		p->in_function = 0;
		p->in_generator = 1;
		p->in_actor = 0;
		break;
	case PARSE_ACT:
		p->in_function = 0;
		p->in_generator = 0;
		p->in_actor = 1;
		break;
	default:
		RHO_INTERNAL_ERROR();
	}

	p->in_lambda = 0;
	p->in_loop = 0;
	RhoAST *body = parse_block(p);
	p->in_function = old_in_function;
	p->in_generator = old_in_generator;
	p->in_actor = old_in_actor;
	p->in_lambda = old_in_lambda;
	p->in_loop = old_in_loop;

	if (RHO_PARSER_ERROR(p)) {
		assert(body == NULL);
		rho_ast_free(name);
		rho_ast_list_free(params);
		return NULL;
	}

	RhoAST *ast;

	switch (select) {
	case PARSE_DEF:
		ast = rho_ast_new(RHO_NODE_DEF, name, body, tok->lineno);
		break;
	case PARSE_GEN:
		ast = rho_ast_new(RHO_NODE_GEN, name, body, tok->lineno);
		break;
	case PARSE_ACT:
		ast = rho_ast_new(RHO_NODE_ACT, name, body, tok->lineno);
		break;
	default:
		RHO_INTERNAL_ERROR();
	}

	ast->v.params = params;
	return ast;
}

static RhoAST *parse_def(RhoParser *p)
{
	return parse_def_or_gen_or_act(p, PARSE_DEF);
}

static RhoAST *parse_gen(RhoParser *p)
{
	return parse_def_or_gen_or_act(p, PARSE_GEN);
}

static RhoAST *parse_act(RhoParser *p)
{
	return parse_def_or_gen_or_act(p, PARSE_ACT);
}

#undef PARSE_DEF
#undef PARSE_GEN
#undef PARSE_ACT

static RhoAST *parse_break(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_BREAK);
	ERROR_CHECK(p);

	if (!p->in_loop) {
		parse_err_invalid_break(p, tok);
		return NULL;
	}

	RhoAST *ast = rho_ast_new(RHO_NODE_BREAK, NULL, NULL, tok->lineno);
	return ast;
}

static RhoAST *parse_continue(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_CONTINUE);
	ERROR_CHECK(p);

	if (!p->in_loop) {
		parse_err_invalid_continue(p, tok);
		return NULL;
	}

	RhoAST *ast = rho_ast_new(RHO_NODE_CONTINUE, NULL, NULL, tok->lineno);
	return ast;
}

static RhoAST *parse_return(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_RETURN);
	ERROR_CHECK(p);

	if (!(p->in_function || p->in_generator || p->in_actor)) {
		parse_err_invalid_return(p, tok);
		return NULL;
	}

	RhoToken *next = rho_parser_peek_token_direct(p);
	RhoAST *ast;

	if (RHO_TOK_TYPE_IS_STMT_TERM(next->type)) {
		ast = rho_ast_new(RHO_NODE_RETURN, NULL, NULL, tok->lineno);
	} else {
		if (p->in_generator) {
			parse_err_return_val_in_gen(p, tok);
			return NULL;
		}

		RhoAST *expr = parse_expr_no_assign(p);
		ERROR_CHECK(p);
		ast = rho_ast_new(RHO_NODE_RETURN, expr, NULL, tok->lineno);
	}

	return ast;
}

static RhoAST *parse_throw(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_THROW);
	ERROR_CHECK(p);
	RhoAST *expr = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_THROW, expr, NULL, tok->lineno);
	return ast;
}

static RhoAST *parse_produce(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_PRODUCE);
	ERROR_CHECK(p);

	if (!p->in_generator) {
		parse_err_invalid_produce(p, tok);
		return NULL;
	}

	RhoAST *expr = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_PRODUCE, expr, NULL, tok->lineno);
	return ast;
}

static RhoAST *parse_receive(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_RECEIVE);
	ERROR_CHECK(p);

	if (!p->in_actor) {
		parse_err_invalid_receive(p, tok);
		return NULL;
	}

	RhoAST *ident = parse_ident(p);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_RECEIVE, ident, NULL, tok->lineno);
	return ast;
}

static RhoAST *parse_try_catch(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_TRY);
	ERROR_CHECK(p);
	RhoAST *try_body = parse_block(p);
	ERROR_CHECK(p);
	RhoToken *catch = expect(p, RHO_TOK_CATCH);
	ERROR_CHECK_AST(p, catch, try_body);

	unsigned int count;
	struct rho_ast_list *exc_list = parse_comma_separated_list(p,
	                                                           RHO_TOK_PAREN_OPEN,
	                                                           RHO_TOK_PAREN_CLOSE,
	                                                           parse_expr,
	                                                           &count);

	ERROR_CHECK_AST(p, exc_list, try_body);

	if (count == 0) {
		parse_err_empty_catch(p, catch);
		rho_ast_free(try_body);
		rho_ast_list_free(exc_list);
		return NULL;
	}

	RhoAST *catch_body = parse_block(p);

	if (RHO_PARSER_ERROR(p)) {
		assert(catch_body == NULL);
		rho_ast_free(try_body);
		rho_ast_list_free(exc_list);
	}

	RhoAST *ast = rho_ast_new(RHO_NODE_TRY_CATCH, try_body, catch_body, tok->lineno);
	ast->v.excs = exc_list;
	return ast;
}

static RhoAST *parse_import(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_IMPORT);
	ERROR_CHECK(p);
	RhoAST *ident = parse_ident(p);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_IMPORT, ident, NULL, tok->lineno);
	return ast;
}

static RhoAST *parse_export(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_EXPORT);
	ERROR_CHECK(p);
	RhoAST *ident = parse_ident(p);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_EXPORT, ident, NULL, tok->lineno);
	return ast;
}

static RhoAST *parse_block(RhoParser *p)
{
	RhoBlock *block_head = NULL;

	RhoToken *peek = rho_parser_peek_token(p);

	RhoToken *brace_open;

	if (peek->type == RHO_TOK_COLON) {
		brace_open = expect(p, RHO_TOK_COLON);
		ERROR_CHECK(p);
		block_head = rho_ast_list_new();
		block_head->ast = parse_stmt(p);
		ERROR_CHECK_LIST(p, block_head->ast, block_head);
	} else {
		brace_open = expect(p, RHO_TOK_BRACE_OPEN);
		ERROR_CHECK(p);
		RhoBlock *block = NULL;

		do {
			RhoToken *next = rho_parser_peek_token(p);

			if (next->type == RHO_TOK_EOF) {
				parse_err_unclosed(p, brace_open);
				return NULL;
			}

			if (next->type == RHO_TOK_BRACE_CLOSE) {
				break;
			}

			RhoAST *stmt = parse_stmt(p);
			ERROR_CHECK_LIST(p, stmt, block_head);

			/*
			 * We don't include empty statements
			 * in the syntax tree.
			 */
			if (stmt->type == RHO_NODE_EMPTY) {
				free(stmt);
				continue;
			}

			if (block_head == NULL) {
				block_head = rho_ast_list_new();
				block = block_head;
			}

			if (block->ast != NULL) {
				block->next = rho_ast_list_new();
				block = block->next;
			}

			block->ast = stmt;
		} while (true);

		expect(p, RHO_TOK_BRACE_CLOSE);
		ERROR_CHECK_LIST(p, NULL, block_head);
	}

	RhoAST *ast = rho_ast_new(RHO_NODE_BLOCK, NULL, NULL, brace_open->lineno);
	ast->v.block = block_head;
	return ast;
}

static RhoAST *parse_list(RhoParser *p)
{
	RhoToken *brack_open = rho_parser_peek_token(p);  /* parse_comma_separated_list does error checking */
	struct rho_ast_list *list_head = parse_comma_separated_list(p,
	                                                            RHO_TOK_BRACK_OPEN,
	                                                            RHO_TOK_BRACK_CLOSE,
	                                                            parse_expr,
	                                                            NULL);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_LIST, NULL, NULL, brack_open->lineno);
	ast->v.list = list_head;
	return ast;
}

static RhoAST *parse_dict_or_set_sub_element(RhoParser *p)
{
	RhoAST *k = parse_expr(p);
	ERROR_CHECK(p);

	RhoToken *sep = rho_parser_peek_token(p);

	if (sep->type == RHO_TOK_COLON) {
		/* dict */
		expect(p, RHO_TOK_COLON);
		RhoAST *v = parse_expr(p);
		ERROR_CHECK_AST(p, v, k);
		return rho_ast_new(RHO_NODE_DICT_ELEM, k, v, k->lineno);
	} else {
		/* set */
		return k;
	}
}

static RhoAST *parse_set_or_dict(RhoParser *p)
{
	RhoToken *brace_open = rho_parser_peek_token(p);
	const unsigned int lineno = brace_open->lineno;
	struct rho_ast_list *head = parse_comma_separated_list(p,
	                                                       RHO_TOK_BRACE_OPEN,
	                                                       RHO_TOK_BRACE_CLOSE,
	                                                       parse_dict_or_set_sub_element,
	                                                       NULL);
	ERROR_CHECK(p);

	RhoAST *ast;

	if (head != NULL) {
		/* check if the elements are consistent */
		const bool is_dict = (head->ast->type == RHO_NODE_DICT_ELEM);

		for (struct rho_ast_list *node = head; node != NULL; node = node->next) {
			if (is_dict ^ (node->ast->type == RHO_NODE_DICT_ELEM)) {
				parse_err_inconsistent_dict_elements(p, brace_open);
				ERROR_CHECK_LIST(p, NULL, head);
			}
		}

		ast = rho_ast_new(is_dict ? RHO_NODE_DICT : RHO_NODE_SET, NULL, NULL, lineno);
	} else {
		ast = rho_ast_new(RHO_NODE_DICT, NULL, NULL, lineno);
	}

	ast->v.list = head;
	return ast;
}

static RhoAST *parse_lambda(RhoParser *p)
{
	RhoToken *colon = rho_parser_peek_token(p);
	expect(p, RHO_TOK_COLON);
	ERROR_CHECK(p);

	const unsigned old_max_dollar_ident = p->max_dollar_ident;
	const unsigned old_in_function = p->in_function;
	const unsigned old_in_generator = p->in_generator;
	const unsigned old_in_actor = p->in_actor;
	const unsigned old_in_lambda = p->in_lambda;
	const unsigned old_in_loop = p->in_loop;
	p->max_dollar_ident = 0;
	p->in_function = 1;
	p->in_generator = 0;
	p->in_actor = 0;
	p->in_loop = 0;
	p->in_lambda = 1;
	RhoAST *body = parse_expr(p);
	const unsigned int max_dollar_ident = p->max_dollar_ident;
	p->max_dollar_ident = old_max_dollar_ident;
	p->in_function = old_in_function;
	p->in_generator = old_in_generator;
	p->in_actor = old_in_actor;
	p->in_lambda = old_in_lambda;
	p->in_loop = old_in_loop;

	ERROR_CHECK(p);

	RhoAST *ast = rho_ast_new(RHO_NODE_LAMBDA, body, NULL, colon->lineno);
	ast->v.max_dollar_ident = max_dollar_ident;
	return ast;
}

static RhoAST *parse_empty(RhoParser *p)
{
	RhoToken *tok = expect(p, RHO_TOK_SEMICOLON);
	ERROR_CHECK(p);
	RhoAST *ast = rho_ast_new(RHO_NODE_EMPTY, NULL, NULL, tok->lineno);
	return ast;
}

/*
 * Parses generic comma-separated list with given start and end delimiters.
 * Returns the number of elements in this parsed list.
 */
static struct rho_ast_list *parse_comma_separated_list(RhoParser *p,
                                                       const RhoTokType open_type, const RhoTokType close_type,
                                                       RhoAST *(*sub_element_parse_routine)(RhoParser *),
                                                       unsigned int *count)
{
	RhoToken *tok_open = expect(p, open_type);
	ERROR_CHECK(p);
	struct rho_ast_list *list_head = NULL;
	struct rho_ast_list *list = NULL;

	unsigned int nelements = 0;

	do {
		RhoToken *next = rho_parser_peek_token(p);

		if (next->type == RHO_TOK_EOF) {
			parse_err_unclosed(p, tok_open);
			ERROR_CHECK_LIST(p, NULL, list_head);
		}

		if (next->type == close_type) {
			break;
		}

		if (list_head == NULL) {
			list_head = rho_ast_list_new();
			list = list_head;
		}

		if (list->ast != NULL) {
			list->next = rho_ast_list_new();
			list = list->next;
		}

		list->ast = sub_element_parse_routine(p);
		ERROR_CHECK_LIST(p, list->ast, list_head);
		++nelements;

		next = rho_parser_peek_token(p);

		if (next->type == RHO_TOK_COMMA) {
			expect(p, RHO_TOK_COMMA);
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

static Op op_from_tok_type(RhoTokType type)
{
	for (size_t i = 0; i < ops_size; i++) {
		if (type == ops[i].type) {
			return ops[i];
		}
	}

	RHO_INTERNAL_ERROR();
	static const Op sentinel;
	return sentinel;
}

static RhoNodeType nodetype_from_op(Op op)
{
	switch (op.type) {
	case RHO_TOK_PLUS:
		return RHO_NODE_ADD;
	case RHO_TOK_MINUS:
		return RHO_NODE_SUB;
	case RHO_TOK_MUL:
		return RHO_NODE_MUL;
	case RHO_TOK_DIV:
		return RHO_NODE_DIV;
	case RHO_TOK_MOD:
		return RHO_NODE_MOD;
	case RHO_TOK_POW:
		return RHO_NODE_POW;
	case RHO_TOK_BITAND:
		return RHO_NODE_BITAND;
	case RHO_TOK_BITOR:
		return RHO_NODE_BITOR;
	case RHO_TOK_XOR:
		return RHO_NODE_XOR;
	case RHO_TOK_BITNOT:
		return RHO_NODE_BITNOT;
	case RHO_TOK_SHIFTL:
		return RHO_NODE_SHIFTL;
	case RHO_TOK_SHIFTR:
		return RHO_NODE_SHIFTR;
	case RHO_TOK_AND:
		return RHO_NODE_AND;
	case RHO_TOK_OR:
		return RHO_NODE_OR;
	case RHO_TOK_NOT:
		return RHO_NODE_NOT;
	case RHO_TOK_EQUAL:
		return RHO_NODE_EQUAL;
	case RHO_TOK_NOTEQ:
		return RHO_NODE_NOTEQ;
	case RHO_TOK_LT:
		return RHO_NODE_LT;
	case RHO_TOK_GT:
		return RHO_NODE_GT;
	case RHO_TOK_LE:
		return RHO_NODE_LE;
	case RHO_TOK_GE:
		return RHO_NODE_GE;
	case RHO_TOK_AT:
		return RHO_NODE_APPLY;
	case RHO_TOK_DOT:
		return RHO_NODE_DOT;
	case RHO_TOK_DOTDOT:
		return RHO_NODE_DOTDOT;
	case RHO_TOK_ASSIGN:
		return RHO_NODE_ASSIGN;
	case RHO_TOK_ASSIGN_ADD:
		return RHO_NODE_ASSIGN_ADD;
	case RHO_TOK_ASSIGN_SUB:
		return RHO_NODE_ASSIGN_SUB;
	case RHO_TOK_ASSIGN_MUL:
		return RHO_NODE_ASSIGN_MUL;
	case RHO_TOK_ASSIGN_DIV:
		return RHO_NODE_ASSIGN_DIV;
	case RHO_TOK_ASSIGN_MOD:
		return RHO_NODE_ASSIGN_MOD;
	case RHO_TOK_ASSIGN_POW:
		return RHO_NODE_ASSIGN_POW;
	case RHO_TOK_ASSIGN_BITAND:
		return RHO_NODE_ASSIGN_BITAND;
	case RHO_TOK_ASSIGN_BITOR:
		return RHO_NODE_ASSIGN_BITOR;
	case RHO_TOK_ASSIGN_XOR:
		return RHO_NODE_ASSIGN_XOR;
	case RHO_TOK_ASSIGN_SHIFTL:
		return RHO_NODE_ASSIGN_SHIFTL;
	case RHO_TOK_ASSIGN_SHIFTR:
		return RHO_NODE_ASSIGN_SHIFTR;
	case RHO_TOK_ASSIGN_AT:
		return RHO_NODE_ASSIGN_APPLY;
	case RHO_TOK_IN:
		return RHO_NODE_IN;
	case RHO_TOK_IF:
		return RHO_NODE_COND_EXPR;
	default:
		RHO_INTERNAL_ERROR();
		return -1;
	}
}

static RhoToken *expect(RhoParser *p, RhoTokType type)
{
	assert(type != RHO_TOK_NONE);
	RhoToken *next = rho_parser_has_next_token(p) ? rho_parser_next_token(p) : NULL;
	const RhoTokType next_type = next ? next->type : RHO_TOK_NONE;

	if (next_type != type) {
		parse_err_unexpected_token(p, next);
		ERROR_CHECK(p);
	}

	return next;
}

/*
 * Parser error functions
 */

static const char *err_on_tok(RhoParser *p, RhoToken *tok)
{
	return rho_err_on_char(tok->value, p->code, p->end, tok->lineno);
}

static void parse_err_unexpected_token(RhoParser *p, RhoToken *tok)
{
#define MAX_LEN 1024

	if (tok->type == RHO_TOK_EOF) {
		/*
		 * This should really always be true,
		 * since we shouldn't have an unexpected
		 * token in an empty file.
		 */
		if (p->tok_count > 1) {
			const char *tok_err = "";
			bool free_tok_err = false;
			RhoToken *tokens = p->tokens;
			size_t last = p->tok_count - 2;

			while (last > 0 && tokens[last].type == RHO_TOK_NEWLINE) {
				--last;
			}

			if (tokens[last].type != RHO_TOK_NEWLINE) {
				tok_err = err_on_tok(p, &tokens[last]);
				free_tok_err = true;
			}

			RHO_PARSER_SET_ERROR_MSG(p,
			                         rho_util_str_format(RHO_SYNTAX_ERROR " unexpected end-of-file after token\n\n%s",
			                            p->name, tok->lineno, tok_err));

			if (free_tok_err) {
				RHO_FREE(tok_err);
			}
		} else {
			RHO_PARSER_SET_ERROR_MSG(p,
			                         rho_util_str_format(RHO_SYNTAX_ERROR " unexpected end-of-file after token\n\n",
			                            p->name, tok->lineno));
		}
	} else {
		char tok_str[MAX_LEN];

		const size_t tok_len = (tok->length < (MAX_LEN - 1)) ? tok->length : (MAX_LEN - 1);
		memcpy(tok_str, tok->value, tok_len);
		tok_str[tok_len] = '\0';

		const char *tok_err = err_on_tok(p, tok);
		RHO_PARSER_SET_ERROR_MSG(p,
		                         rho_util_str_format(RHO_SYNTAX_ERROR " unexpected token: %s\n\n%s",
		                            p->name, tok->lineno, tok_str, tok_err));
		RHO_FREE(tok_err);
	}

	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_UNEXPECTED_TOKEN);

#undef MAX_LEN
}

static void parse_err_not_a_statement(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " not a statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_NOT_A_STATEMENT);
}

static void parse_err_unclosed(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " unclosed\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_UNCLOSED);
}

static void parse_err_invalid_assign(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " misplaced assignment\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_INVALID_ASSIGN);
}

static void parse_err_invalid_break(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " misplaced break statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_INVALID_BREAK);
}

static void parse_err_invalid_continue(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " misplaced continue statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_INVALID_CONTINUE);
}

static void parse_err_invalid_return(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " misplaced return statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_INVALID_RETURN);
}

static void parse_err_invalid_produce(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " misplaced produce statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_INVALID_PRODUCE);
}

static void parse_err_invalid_receive(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " misplaced receive statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_INVALID_RECEIVE);
}

static void parse_err_too_many_params(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " function has too many parameters (max %d)\n\n%s",
	                            p->name, tok->lineno, FUNCTION_MAX_PARAMS, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_TOO_MANY_PARAMETERS);
}

static void parse_err_dup_params(RhoParser *p, RhoToken *tok, const char *param)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " function has duplicate parameter '%s'\n\n%s",
	                            p->name, tok->lineno, param, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_DUPLICATE_PARAMETERS);
}

static void parse_err_non_default_after_default(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " non-default parameter after default parameter\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_NON_DEFAULT_AFTER_DEFAULT_PARAMETERS);
}

static void parse_err_malformed_params(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " function has malformed parameters\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_MALFORMED_PARAMETERS);
}

static void parse_err_too_many_args(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " function call has too many arguments (max %d)\n\n%s",
	                            p->name, tok->lineno, FUNCTION_MAX_PARAMS, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_TOO_MANY_ARGUMENTS);
}

static void parse_err_dup_named_args(RhoParser *p, RhoToken *tok, const char *name)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " function call has duplicate named argument '%s'\n\n%s",
	                            p->name, tok->lineno, name, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_DUPLICATE_NAMED_ARGUMENTS);
}

static void parse_err_unnamed_after_named(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " unnamed arguments after named arguments\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_UNNAMED_AFTER_NAMED_ARGUMENTS);
}

static void parse_err_malformed_args(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " function call has malformed arguments\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_MALFORMED_ARGUMENTS);
}

static void parse_err_empty_catch(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " empty catch statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_EMPTY_CATCH);
}

static void parse_err_misplaced_dollar_identifier(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " dollar identifier outside lambda\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_MISPLACED_DOLLAR_IDENTIFIER);
}

static void parse_err_inconsistent_dict_elements(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " inconsistent dictionary elements\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_INCONSISTENT_DICT_ELEMENTS);
}

static void parse_err_empty_for_params(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " empty for-loop parameter list\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_EMPTY_FOR_PARAMETERS);
}

static void parse_err_return_val_in_gen(RhoParser *p, RhoToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " generators cannot return a value\n\n%s",
	                            p->name, tok->lineno, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_RETURN_VALUE_IN_GENERATOR);
}
