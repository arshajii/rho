#ifndef LEXER_H
#define LEXER_H

#include <stdlib.h>
#include <stdbool.h>

typedef enum {
	TOK_NONE,           // indicates absent or unknown type

	/* literals */
	TOK_INT,            // int literal
	TOK_FLOAT,          // float literal
	TOK_STR,            // quoted string: ".*" (escapes are allowed too)
	TOK_IDENT,          // [a-zA-Z_][a-zA-Z_0-9]*

	/* operators */
	TOK_OPS_START,  /* marker */
	TOK_PLUS,
	TOK_MINUS,
	TOK_MUL,
	TOK_DIV,
	TOK_MOD,
	TOK_POW,
	TOK_BITAND,
	TOK_BITOR,
	TOK_XOR,
	TOK_BITNOT,
	TOK_SHIFTL,
	TOK_SHIFTR,
	TOK_AND,
	TOK_OR,
	TOK_NOT,
	TOK_EQUAL,
	TOK_NOTEQ,
	TOK_LT,
	TOK_GT,
	TOK_LE,
	TOK_GE,
	TOK_DOT,

	/* assignments */
	TOK_ASSIGNMENTS_START,
	TOK_ASSIGN,
	TOK_ASSIGN_ADD,
	TOK_ASSIGN_SUB,
	TOK_ASSIGN_MUL,
	TOK_ASSIGN_DIV,
	TOK_ASSIGN_MOD,
	TOK_ASSIGN_POW,
	TOK_ASSIGN_BITAND,
	TOK_ASSIGN_BITOR,
	TOK_ASSIGN_XOR,
	TOK_ASSIGN_SHIFTL,
	TOK_ASSIGN_SHIFTR,
	TOK_ASSIGNMENTS_END,
	TOK_OPS_END,    /* marker */

	TOK_PAREN_OPEN,     // literal: (
	TOK_PAREN_CLOSE,    // literal: )
	TOK_BRACE_OPEN,     // literal: {
	TOK_BRACE_CLOSE,    // literal: }
	TOK_BRACK_OPEN,     // literal: [
	TOK_BRACK_CLOSE,    // literal: ]

	/* keywords */
	TOK_PRINT,
	TOK_IF,
	TOK_ELIF,
	TOK_ELSE,
	TOK_WHILE,
	TOK_DEF,
	TOK_BREAK,
	TOK_CONTINUE,
	TOK_RETURN,
	TOK_THROW,
	TOK_TRY,
	TOK_CATCH,

	/* miscellaneous tokens */
	TOK_COMMA,
	TOK_COLON,

	/* statement terminators */
	TOK_SEMICOLON,
	TOK_NEWLINE,
	TOK_EOF             // should always be the last token
} TokType;

#define IS_OP(type) (TOK_OPS_START < (type) && (type) < TOK_OPS_END)

#define IS_ASSIGNMENT_TOK(type) \
	(TOK_ASSIGNMENTS_START < (type) && (type) < TOK_ASSIGNMENTS_END)

typedef struct {
	const char *value;    // not null-terminated
	size_t length;
	TokType type;
	unsigned int lineno;  // 1-based line number
} Token;

void print_token(Token *tok);

typedef struct {
	/* source code to parse */
	const char *code;

	/* end of source code (last char) */
	const char *end;

	/* where we are in the string */
	char *pos;

	/* increases to consume token */
	unsigned int mark;

	/* tokens that have been read */
	Token *tokens;
	size_t tok_count;
	size_t tok_capacity;

	/* the "peek-token" is somewhat complicated to
	   compute, so we cache it */
	Token *peek;

	/* where we are in the tokens array */
	size_t tok_pos;

	/* the line number/position we are currently on */
	unsigned int lineno;

	/* name of the file out of which the source was read */
	const char *name;

	/* parse flags */
	unsigned in_function : 1;
	unsigned in_loop : 1;
} Lexer;

Lexer *lex_new(char *str, const size_t length, const char *name);
Token *lex_next_token(Lexer *lex);
Token *lex_next_token_direct(Lexer *lex);
Token *lex_peek_token(Lexer *lex);
Token *lex_peek_token_direct(Lexer *lex);
bool lex_has_next(Lexer *lex);
void lex_free(Lexer *lex);

const char *type_to_str(TokType type);

#endif /* LEXER_H */
