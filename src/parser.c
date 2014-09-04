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

const size_t ops_size = (sizeof(ops) / sizeof(Op));

#define FUNCTION_MAX_PARAMS 128

static AST *parse_stmt(Lexer *lex);

static AST *parse_expr(Lexer *lex);
static AST *parse_subexpr(Lexer *lex);
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
static AST *parse_return(Lexer *lex);

static AST *parse_block(Lexer *lex);

static AST *parse_empty(Lexer *lex);

static Token *expect(Lexer *lex, TokType type);

static void parse_err_unexpected_token(Lexer *lex, Token *tok);
static void parse_err_not_a_statement(Lexer *lex, Token *tok);
static void parse_err_unclosed(Lexer *lex, Token *tok);
static void parse_err_invalid_assign(Lexer *lex, Token *tok);
static void parse_err_invalid_return(Lexer *lex, Token *tok);
static void parse_err_too_many_params(Lexer *lex, Token *tok);

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
	case TOK_RETURN:
		stmt = parse_return(lex);
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
	    stmt_end_type != TOK_BRACKET_CLOSE) {

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

		AST *ast = ast_new();
		ast->type = nodetype_from_op(op);
		ast->left = lhs;
		ast->right = rhs;

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
	case TOK_PAREN_OPEN: {
		ast = parse_subexpr(lex);
		break;
	}
	case TOK_INT: {
		ast = parse_int(lex);
		break;
	}
	case TOK_FLOAT: {
		ast = parse_float(lex);
		break;
	}
	case TOK_STR: {
		ast = parse_str(lex);
		break;
	}
	case TOK_IDENT: {
		ast = parse_ident(lex);
		break;
	}
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
		expect(lex, TOK_DOT);

		AST *dot = ast_new();
		dot->type = NODE_DOT;
		dot->left = ast;
		dot->right = parse_ident(lex);
		ast = dot;

		tok = lex_peek_token(lex);
	}

	/*
	 * Deal with function calls...
	 */
	while (tok->type == TOK_PAREN_OPEN) {
		Token *paren_open = expect(lex, TOK_PAREN_OPEN);

		ParamList *params_head = NULL;
		ParamList *params = NULL;

		do {
			Token *next = lex_peek_token(lex);

			if (next->type == TOK_EOF) {
				parse_err_unclosed(lex, paren_open);
			}

			if (next->type == TOK_PAREN_CLOSE) {
				break;
			}

			if (params_head == NULL) {
				params_head = ast_list_new();
				params = params_head;
			}

			if (params->ast != NULL) {
				params->next = ast_list_new();
				params = params->next;
			}

			params->ast = parse_expr(lex);

			next = lex_peek_token(lex);

			if (next->type == TOK_COMMA) {
				expect(lex, TOK_COMMA);

				next = lex_peek_token(lex);
				if (next->type == TOK_PAREN_CLOSE) {
					parse_err_unexpected_token(lex, next);
				}
			} else if (next->type != TOK_PAREN_CLOSE) {
				parse_err_unexpected_token(lex, next);
			}
		} while (true);

		expect(lex, TOK_PAREN_CLOSE);

		AST *call = ast_new();
		call->type = NODE_CALL;
		call->v.params = params_head;
		call->left = ast;
		call->right = NULL;
		ast = call;

		tok = lex_peek_token(lex);
	}

	end:
	return ast;
}

/*
 * Parses parenthesized expression.
 */
static AST *parse_subexpr(Lexer *lex)
{
	expect(lex, TOK_PAREN_OPEN);
	AST *ast = parse_expr(lex);
	expect(lex, TOK_PAREN_CLOSE);

	return ast;
}

static AST *parse_unop(Lexer *lex)
{
	Token *tok = lex_next_token(lex);

	NodeType type = NODE_EMPTY;
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
		INTERNAL_ERROR();
		break;
	}

	AST *ast = ast_new();
	ast->type = type;
	ast->left = parse_atom(lex);
	ast->right = NULL;

	return ast;
}

static AST *parse_int(Lexer *lex)
{
	Token *tok = expect(lex, TOK_INT);

	AST *ast = ast_new();
	ast->type = NODE_INT;
	ast->v.int_val = atoi(tok->value);
	ast->left = ast->right = NULL;

	return ast;
}

static AST *parse_float(Lexer *lex)
{
	Token *tok = expect(lex, TOK_FLOAT);

	AST *ast = ast_new();
	ast->type = NODE_FLOAT;
	ast->v.float_val = atof(tok->value);
	ast->left = ast->right = NULL;

	return ast;
}

static AST *parse_str(Lexer *lex)
{
	Token *tok = expect(lex, TOK_STR);

	AST *ast = ast_new();
	ast->type = NODE_STRING;

	// deal with quotes appropriately:
	ast->v.str_val = str_new_copy(tok->value + 1, tok->length - 2);

	ast->left = ast->right = NULL;

	return ast;
}

static AST *parse_ident(Lexer *lex)
{
	Token *tok = expect(lex, TOK_IDENT);
	AST *ast = ast_new();

	ast->type = NODE_IDENT;
	ast->v.ident = str_new_copy(tok->value, tok->length);
	ast->left = ast->right = NULL;

	return ast;
}

static AST *parse_print(Lexer *lex)
{
	expect(lex, TOK_PRINT);
	AST *ast = ast_new();
	ast->type = NODE_PRINT;
	ast->left = parse_expr(lex);
	ast->right = NULL;

	return ast;
}

static AST *parse_if(Lexer *lex)
{
	expect(lex, TOK_IF);
	AST *ast = ast_new();
	ast->type = NODE_IF;
	ast->left = parse_expr(lex);   // cond
	ast->right = parse_block(lex); // body

	if (lex_peek_token(lex)->type == TOK_ELSE) {
		expect(lex, TOK_ELSE);
		ast->v.middle = parse_block(lex);  // else body
	} else {
		ast->v.middle = NULL;
	}

	return ast;
}

static AST *parse_while(Lexer *lex)
{
	expect(lex, TOK_WHILE);
	AST *ast = ast_new();
	ast->type = NODE_WHILE;
	ast->left = parse_expr(lex);   // cond

	unsigned old_in_loop = lex->in_loop;
	lex->in_loop = 1;
	ast->right = parse_block(lex); // body
	lex->in_loop = old_in_loop;

	return ast;
}

static AST *parse_def(Lexer *lex)
{
	expect(lex, TOK_DEF);
	AST *ast = ast_new();
	ast->type = NODE_DEF;
	Token *name = lex_peek_token(lex);
	ast->left = parse_ident(lex);  // name

	Token *paren_open = expect(lex, TOK_PAREN_OPEN);

	ParamList *params_head = NULL;
	ParamList *params = NULL;

	unsigned int nargs = 0;

	do {
		Token *next = lex_peek_token(lex);

		if (next->type == TOK_EOF) {
			parse_err_unclosed(lex, paren_open);
		}

		if (next->type == TOK_PAREN_CLOSE) {
			break;
		}

		if (params_head == NULL) {
			params_head = ast_list_new();
			params = params_head;
		}

		if (params->ast != NULL) {
			params->next = ast_list_new();
			params = params->next;
		}

		params->ast = parse_ident(lex);
		++nargs;

		next = lex_peek_token(lex);

		if (next->type == TOK_COMMA) {
			expect(lex, TOK_COMMA);

			next = lex_peek_token(lex);
			if (next->type == TOK_PAREN_CLOSE) {
				parse_err_unexpected_token(lex, next);
			}
		} else if (next->type != TOK_PAREN_CLOSE) {
			parse_err_unexpected_token(lex, next);
		}
	} while (true);

	expect(lex, TOK_PAREN_CLOSE);

	ast->v.params = params_head;

	unsigned old_in_function = lex->in_function;
	lex->in_function = 1;
	ast->right = parse_block(lex);  // body
	lex->in_function = old_in_function;

	if (nargs > FUNCTION_MAX_PARAMS) {
		parse_err_too_many_params(lex, name);
	}

	return ast;
}

static AST *parse_return(Lexer *lex)
{
	Token *return_tok = expect(lex, TOK_RETURN);

	if (!lex->in_function) {
		parse_err_invalid_return(lex, return_tok);
	}

	AST *ast = ast_new();
	ast->type = NODE_RETURN;
	ast->left = parse_expr(lex);
	ast->right = NULL;
	return ast;
}

static AST *parse_block(Lexer *lex)
{
	Token *bracket_open = expect(lex, TOK_BRACKET_OPEN);

	Block *block_head = ast_list_new();
	Block *block = block_head;

	do {
		Token *next = lex_peek_token(lex);

		if (next->type == TOK_EOF) {
			parse_err_unclosed(lex, bracket_open);
		}

		if (next->type == TOK_BRACKET_CLOSE) {
			break;
		}

		if (block->ast != NULL) {
			block->next = ast_list_new();
			block = block->next;
		}

		block->ast = parse_stmt(lex);
	} while (true);

	expect(lex, TOK_BRACKET_CLOSE);

	AST *ast = ast_new();
	ast->type = NODE_BLOCK;
	ast->v.block = block_head;
	ast->left = ast->right = NULL;

	return ast;
}

static AST *parse_empty(Lexer *lex)
{
	expect(lex, TOK_SEMICOLON);
	AST *ast = ast_new();
	ast->type = NODE_EMPTY;
	ast->left = NULL;
	ast->right = NULL;
	return ast;
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
