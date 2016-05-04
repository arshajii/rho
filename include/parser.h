#ifndef RHO_PARSER_H
#define RHO_PARSER_H

#include "ast.h"

typedef enum {
	RHO_TOK_NONE,           // indicates absent or unknown type

	/* literals */
	RHO_TOK_INT,            // int literal
	RHO_TOK_FLOAT,          // float literal
	RHO_TOK_STR,            // quoted string: ".*" (escapes are allowed too)
	RHO_TOK_IDENT,          // [a-zA-Z_][a-zA-Z_0-9]*

	/* operators */
	RHO_TOK_OPS_START,  /* marker */
	RHO_TOK_PLUS,
	RHO_TOK_MINUS,
	RHO_TOK_MUL,
	RHO_TOK_DIV,
	RHO_TOK_MOD,
	RHO_TOK_POW,
	RHO_TOK_BITAND,
	RHO_TOK_BITOR,
	RHO_TOK_XOR,
	RHO_TOK_BITNOT,
	RHO_TOK_SHIFTL,
	RHO_TOK_SHIFTR,
	RHO_TOK_AND,
	RHO_TOK_OR,
	RHO_TOK_NOT,
	RHO_TOK_EQUAL,
	RHO_TOK_NOTEQ,
	RHO_TOK_LT,
	RHO_TOK_GT,
	RHO_TOK_LE,
	RHO_TOK_GE,
	RHO_TOK_AT,
	RHO_TOK_DOT,
	RHO_TOK_DOTDOT,
	RHO_TOK_IN,  // really a keyword but treated as an operator in some contexts

	/* assignments */
	RHO_TOK_ASSIGNMENTS_START,
	RHO_TOK_ASSIGN,
	RHO_TOK_ASSIGN_ADD,
	RHO_TOK_ASSIGN_SUB,
	RHO_TOK_ASSIGN_MUL,
	RHO_TOK_ASSIGN_DIV,
	RHO_TOK_ASSIGN_MOD,
	RHO_TOK_ASSIGN_POW,
	RHO_TOK_ASSIGN_BITAND,
	RHO_TOK_ASSIGN_BITOR,
	RHO_TOK_ASSIGN_XOR,
	RHO_TOK_ASSIGN_SHIFTL,
	RHO_TOK_ASSIGN_SHIFTR,
	RHO_TOK_ASSIGN_AT,
	RHO_TOK_ASSIGNMENTS_END,
	RHO_TOK_OPS_END,    /* marker */

	RHO_TOK_PAREN_OPEN,     // literal: (
	RHO_TOK_PAREN_CLOSE,    // literal: )
	RHO_TOK_BRACE_OPEN,     // literal: {
	RHO_TOK_BRACE_CLOSE,    // literal: }
	RHO_TOK_BRACK_OPEN,     // literal: [
	RHO_TOK_BRACK_CLOSE,    // literal: ]

	/* keywords */
	RHO_TOK_PRINT,
	RHO_TOK_IF,
	RHO_TOK_ELIF,
	RHO_TOK_ELSE,
	RHO_TOK_WHILE,
	RHO_TOK_FOR,
	RHO_TOK_DEF,
	RHO_TOK_BREAK,
	RHO_TOK_CONTINUE,
	RHO_TOK_RETURN,
	RHO_TOK_THROW,
	RHO_TOK_TRY,
	RHO_TOK_CATCH,
	RHO_TOK_IMPORT,
	RHO_TOK_EXPORT,

	/* miscellaneous tokens */
	RHO_TOK_COMMA,
	RHO_TOK_COLON,
	RHO_TOK_DOLLAR,

	/* statement terminators */
	RHO_TOK_SEMICOLON,
	RHO_TOK_NEWLINE,
	RHO_TOK_EOF             // should always be the last token
} RhoTokType;


#define RHO_TOK_TYPE_IS_OP(type) ((RHO_TOK_OPS_START < (type) && (type) < RHO_TOK_OPS_END))

#define RHO_TOK_TYPE_IS_ASSIGNMENT_TOK(type) \
	(RHO_TOK_ASSIGNMENTS_START < (type) && (type) < RHO_TOK_ASSIGNMENTS_END)

typedef struct {
	const char *value;    // not null-terminated
	size_t length;
	RhoTokType type;
	unsigned int lineno;  // 1-based line number
} RhoToken;

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
	RhoToken *tokens;
	size_t tok_count;
	size_t tok_capacity;

	/* the "peek-token" is somewhat complicated to
	   compute, so we cache it */
	RhoToken *peek;

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
} RhoParser;

enum {
	RHO_PARSE_ERR_NONE = 0,
	RHO_PARSE_ERR_UNEXPECTED_CHAR,
	RHO_PARSE_ERR_UNEXPECTED_TOKEN,
	RHO_PARSE_ERR_NOT_A_STATEMENT,
	RHO_PARSE_ERR_UNCLOSED,
	RHO_PARSE_ERR_INVALID_ASSIGN,
	RHO_PARSE_ERR_INVALID_BREAK,
	RHO_PARSE_ERR_INVALID_CONTINUE,
	RHO_PARSE_ERR_INVALID_RETURN,
	RHO_PARSE_ERR_TOO_MANY_PARAMETERS,
	RHO_PARSE_ERR_DUPLICATE_PARAMETERS,
	RHO_PARSE_ERR_NON_DEFAULT_AFTER_DEFAULT_PARAMETERS,
	RHO_PARSE_ERR_MALFORMED_PARAMETERS,
	RHO_PARSE_ERR_TOO_MANY_ARGUMENTS,
	RHO_PARSE_ERR_DUPLICATE_NAMED_ARGUMENTS,
	RHO_PARSE_ERR_UNNAMED_AFTER_NAMED_ARGUMENTS,
	RHO_PARSE_ERR_MALFORMED_ARGUMENTS,
	RHO_PARSE_ERR_EMPTY_CATCH,
	RHO_PARSE_ERR_MISPLACED_DOLLAR_IDENTIFIER
};

RhoParser *rho_parser_new(char *str, const char *name);
void rho_parser_free(RhoParser *p);
RhoProgram *rho_parse(RhoParser *p);

#define RHO_PARSER_SET_ERROR_MSG(p, msg)   (p)->error_msg = (msg)
#define RHO_PARSER_SET_ERROR_TYPE(p, type) (p)->error_type = (type)
#define RHO_PARSER_ERROR(p)                ((p)->error_type != RHO_PARSE_ERR_NONE)

#endif /* RHO_PARSER_H */
