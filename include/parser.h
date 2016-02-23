#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

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
	TOK_AT,
	TOK_DOT,
	TOK_DOTDOT,
	TOK_IN,  // really a keyword but treated as an operator in some contexts

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
	TOK_ASSIGN_AT,
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
	TOK_FOR,
	TOK_DEF,
	TOK_BREAK,
	TOK_CONTINUE,
	TOK_RETURN,
	TOK_THROW,
	TOK_TRY,
	TOK_CATCH,
	TOK_IMPORT,
	TOK_EXPORT,

	/* miscellaneous tokens */
	TOK_COMMA,
	TOK_COLON,
	TOK_DOLLAR,

	/* statement terminators */
	TOK_SEMICOLON,
	TOK_NEWLINE,
	TOK_EOF             // should always be the last token
} TokType;


#define IS_OP(type) ((TOK_OPS_START < (type) && (type) < TOK_OPS_END) || type == TOK_IF)

#define IS_ASSIGNMENT_TOK(type) \
	(TOK_ASSIGNMENTS_START < (type) && (type) < TOK_ASSIGNMENTS_END)

typedef struct {
	const char *value;    // not null-terminated
	size_t length;
	TokType type;
	unsigned int lineno;  // 1-based line number
} Token;

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

	/* if an error occurred... */
	const char *error_msg;
	int error_type;

	/* maximum $N identifier in lambda */
	unsigned int max_dollar_ident;

	/* parse flags */
	unsigned in_function : 1;
	unsigned in_lambda   : 1;
	unsigned in_loop     : 1;
} Parser;

enum {
	PARSE_ERR_NONE = 0,
	PARSE_ERR_UNEXPECTED_CHAR,
	PARSE_ERR_UNEXPECTED_TOKEN,
	PARSE_ERR_NOT_A_STATEMENT,
	PARSE_ERR_UNCLOSED,
	PARSE_ERR_INVALID_ASSIGN,
	PARSE_ERR_INVALID_BREAK,
	PARSE_ERR_INVALID_CONTINUE,
	PARSE_ERR_INVALID_RETURN,
	PARSE_ERR_TOO_MANY_PARAMETERS,
	PARSE_ERR_DUPLICATE_PARAMETERS,
	PARSE_ERR_NON_DEFAULT_AFTER_DEFAULT_PARAMETERS,
	PARSE_ERR_MALFORMED_PARAMETERS,
	PARSE_ERR_TOO_MANY_ARGUMENTS,
	PARSE_ERR_DUPLICATE_NAMED_ARGUMENTS,
	PARSE_ERR_UNNAMED_AFTER_NAMED_ARGUMENTS,
	PARSE_ERR_MALFORMED_ARGUMENTS,
	PARSE_ERR_EMPTY_CATCH,
	PARSE_ERR_MISPLACED_DOLLAR_IDENTIFIER
};

Parser *parser_new(char *str, const char *name);
void parser_free(Parser *p);
Program *parse(Parser *p);

#define PARSER_SET_ERROR_MSG(p, msg)   (p)->error_msg = (msg)
#define PARSER_SET_ERROR_TYPE(p, type) (p)->error_type = (type)
#define PARSER_ERROR(p)                ((p)->error_type != PARSE_ERR_NONE)

#endif /* PARSER_H */
