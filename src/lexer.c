#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "util.h"
#include "err.h"
#include "lexer.h"

/*
 * The lexical analysis stage consists of splitting the input source
 * into tokens. The source is fully tokenized upon the creation of a
 * Lexer instance, which can then be queried to retrieve the tokens in
 * succession. A Lexer can, therefore, be thought of as a simple queue
 * of tokens.
 *
 * Specifically, the following operations are supported:
 *
 * - has_next   :  whether the given Lexer has any more tokens
 *
 * - next_token :  retrieve the next token from the given Lexer and
 *                 advance the Lexer on to the next token
 *
 * - peek_token :  retreive the next token from the given Lexer but
 *                 do not advance the Lexer on to the next token
 *
 * The Lexer structure has a `pos` field and a `mark` field. When a
 * token is encountered, its first character is pointed to by `pos`,
 * and `mark` increases gradually from zero to "consume" the token.
 * Once the token has been read, `pos` is set to the start of the next
 * token and `mark` is set back to 0 so the same process can begin
 * again. This continues until tokenization is complete. Note that
 * `pos` is a character pointer and `mark` is an unsigned integer
 * indicating how many characters (starting from `pos`) have been
 * consumed as part of the token.
 */

#define INITIAL_TOKEN_ARRAY_CAPACITY 5

static void tokenize(Lexer *lex);

static void lex_err_unknown_char(Lexer *lex, const char *c);

static bool is_op_char(const char c)
{
	switch (c) {
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case '&':
	case '|':
	case '^':
	case '!':
	case '~':
	case '=':
	case '<':
	case '>':
	case '.':
		return true;
	default:
		return false;
	}
}

static TokType str_to_op_toktype(const char *str, const size_t len)
{
	if (len == 0)
		return TOK_NONE;

	switch (str[0]) {
	case '+':
		if (len == 1)
			return TOK_PLUS;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_ASSIGN_ADD;
			break;
		}
		break;
	case '-':
		if (len == 1)
			return TOK_MINUS;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_ASSIGN_SUB;
			break;
		}
		break;
	case '*':
		if (len == 1)
			return TOK_MUL;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_ASSIGN_MUL;
			break;
		case '*':
			if (len == 2)
				return TOK_POW;

			switch (str[2]) {
			case '=':
				if (len == 3)
					return TOK_ASSIGN_POW;
				break;
			}
			break;
		}
		break;
	case '/':
		if (len == 1)
			return TOK_DIV;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_ASSIGN_DIV;
			break;
		}
		break;
	case '%':
		if (len == 1)
			return TOK_MOD;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_ASSIGN_MOD;
			break;
		}
		break;
	case '&':
		if (len == 1)
			return TOK_BITAND;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_ASSIGN_BITAND;
			break;
		case '&':
			if (len == 2)
				return TOK_AND;
			break;
		}
		break;
	case '|':
		if (len == 1)
			return TOK_BITOR;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_ASSIGN_BITOR;
			break;
		case '|':
			if (len == 2)
				return TOK_OR;
			break;
		}
		break;
	case '^':
		if (len == 1)
			return TOK_XOR;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_ASSIGN_XOR;
			break;
		}
		break;
	case '!':
		if (len == 1)
			return TOK_NOT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_NOTEQ;
			break;
		}
		break;
	case '~':
		if (len == 1)
			return TOK_BITNOT;

		switch (str[1]) {
		}
		break;
	case '=':
		if (len == 1)
			return TOK_ASSIGN;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_EQUAL;
			break;
		}
		break;
	case '<':
		if (len == 1)
			return TOK_LT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_LE;
			break;
		case '<':
			if (len == 2)
				return TOK_SHIFTL;

			switch (str[2]) {
			case '=':
				if (len == 3)
					return TOK_ASSIGN_SHIFTL;
				break;
			}
			break;
		}
		break;
	case '>':
		if (len == 1)
			return TOK_GT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return TOK_GE;
			break;
		case '>':
			if (len == 2)
				return TOK_SHIFTR;

			switch (str[2]) {
			case '=':
				if (len == 3)
					return TOK_ASSIGN_SHIFTR;
				break;
			}
			break;
		}
		break;
	case '.':
		if (len == 1)
			return TOK_DOT;

		switch (str[1]) {
		}
		break;
	}

	return TOK_NONE;
}

static bool is_word_char(const char c)
{
	return (c == '_' || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'));
}

static bool is_id_char(const char c)
{
	return (c == '_' || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9'));
}

/* debugging */
void print_token(Token *tok)
{
	for (size_t i = 0; i < tok->length; i++) {
		printf("%c", tok->value[i]);
	}
	printf("\n");
}

/*
 * The next few functions have comments that use
 * terminology from the extended comment at the
 * start of this file.
 */

/*
 * Forwards the lexer to its current mark then
 * resets the mark.
 */
static inline void fix(Lexer *lex)
{
	lex->pos += lex->mark + 1;
	lex->mark = 0;
}

/*
 * Returns the lexer's current token based on
 * its position and mark.
 */
static Token get(Lexer *lex, TokType type)
{
	Token tok;
	tok.value = lex->pos;
	tok.length = lex->mark + 1;
	tok.type = type;
	tok.lineno = lex->lineno;

	return tok;
}

/*
 * Returns the lexer's current character.
 */
static inline char currc(Lexer *lex)
{
	return lex->pos[lex->mark];
}

/*
 * Returns the lexer's next character.
 */
static inline char nextc(Lexer *lex)
{
	return lex->pos[lex->mark + 1];
}

/*
 * Advances the lexer mark.
 */
static inline void adv(Lexer *lex)
{
	++lex->mark;
}

/*
 * Forwards the lexer position.
 */
static inline void fwd(Lexer *lex)
{
	++lex->pos;
}

/*
 * Rewinds the lexer mark.
 */
static inline void rew(Lexer *lex)
{
	--lex->mark;
}

static inline bool isspace_except_newline(int c)
{
	return isspace(c) && c != '\n';
}

static void skip_spaces(Lexer *lex)
{
	while (isspace_except_newline(lex->pos[0])) {
		fwd(lex);
	}
}

static void read_digits(Lexer *lex)
{
	while (isdigit(nextc(lex))) {
		adv(lex);
	}
}

static Token next_number(Lexer *lex)
{
	assert(isdigit(currc(lex)));

	TokType type = TOK_INT;
	read_digits(lex);

	if (nextc(lex) == '.') {
		adv(lex);
		read_digits(lex);
		type = TOK_FLOAT;
	}

	Token tok = get(lex, type);
	fix(lex);
	return tok;
}

static Token next_string(Lexer *lex, const char delim)
{
	assert(delim == '"' || delim == '\'');
	assert(currc(lex) == delim);
	adv(lex);  // skip the first quotation character

	unsigned int escape_count = 0;

	do {
		const char c = currc(lex);

		if (c == '\\') {
			++escape_count;
		}
		else if (c == delim && escape_count % 2 == 0) {
			break;
		}
		else {
			escape_count = 0;
		}

		adv(lex);
	} while (true);

	Token tok = get(lex, TOK_STR);

	fix(lex);
	return tok;
}

static Token next_op(Lexer *lex)
{
	assert(is_op_char(currc(lex)));

	/*
	 * Consume all tokens that constitute simple operators,
	 * e.g. '+', '-', '*' etc.
	 */
	while (is_op_char(nextc(lex))) {
		adv(lex);
	}

	/*
	 * Back up until we reach a 'valid' operator,
	 * in accordance with the maximal munch rule.
	 */
	TokType optype;

	while ((optype = str_to_op_toktype(lex->pos, lex->mark + 1)) == TOK_NONE) {
		rew(lex);
	}

	Token tok = get(lex, optype);
	fix(lex);
	return tok;
}

static Token next_word(Lexer *lex)
{
	static const struct {
		const char *keyword;
		TokType type;
	} keywords[] = {
		{"print",    TOK_PRINT},
		{"if",       TOK_IF},
		{"elif",     TOK_ELIF},
		{"else",     TOK_ELSE},
		{"while",    TOK_WHILE},
		{"def",      TOK_DEF},
		{"break",    TOK_BREAK},
		{"continue", TOK_CONTINUE},
		{"return",   TOK_RETURN},
		{"throw",    TOK_THROW},
		{"try",      TOK_TRY},
		{"catch",    TOK_CATCH},
		{"import",   TOK_IMPORT},
		{"export",   TOK_EXPORT}
	};

	assert(is_word_char(currc(lex)));

	while (is_id_char(nextc(lex))) {
		adv(lex);
	}

	Token tok = get(lex, TOK_IDENT);
	fix(lex);

	const size_t keywords_size = sizeof(keywords)/sizeof(keywords[0]);
	const char *word = tok.value;
	const size_t len = tok.length;

	for (size_t i = 0; i < keywords_size; i++) {
		const char *keyword = keywords[i].keyword;
		const size_t kwlen = strlen(keyword);

		if (kwlen == len && strncmp(word, keyword, len) == 0) {
			tok.type = keywords[i].type;
		}
	}

	return tok;
}

static Token next_paren_open(Lexer *lex)
{
	assert(currc(lex) == '(');
	Token tok = get(lex, TOK_PAREN_OPEN);
	fwd(lex);
	return tok;
}

static Token next_paren_close(Lexer *lex)
{
	assert(currc(lex) == ')');
	Token tok = get(lex, TOK_PAREN_CLOSE);
	fwd(lex);
	return tok;
}

static Token next_brace_open(Lexer *lex)
{
	assert(currc(lex) == '{');
	Token tok = get(lex, TOK_BRACE_OPEN);
	fwd(lex);
	return tok;
}

static Token next_brace_close(Lexer *lex)
{
	assert(currc(lex) == '}');
	Token tok = get(lex, TOK_BRACE_CLOSE);
	fwd(lex);
	return tok;
}

static Token next_bracket_open(Lexer *lex)
{
	assert(currc(lex) == '[');
	Token tok = get(lex, TOK_BRACK_OPEN);
	fwd(lex);
	return tok;
}

static Token next_bracket_close(Lexer *lex)
{
	assert(currc(lex) == ']');
	Token tok = get(lex, TOK_BRACK_CLOSE);
	fwd(lex);
	return tok;
}

static Token next_comma(Lexer *lex)
{
	assert(currc(lex) == ',');
	Token tok = get(lex, TOK_COMMA);
	fwd(lex);
	return tok;
}

static Token next_colon(Lexer *lex)
{
	assert(currc(lex) == ':');
	Token tok = get(lex, TOK_COLON);
	fwd(lex);
	return tok;
}

static Token next_semicolon(Lexer *lex)
{
	assert(currc(lex) == ';');
	Token tok = get(lex, TOK_SEMICOLON);
	fwd(lex);
	return tok;
}

static Token next_newline(Lexer *lex)
{
	assert(currc(lex) == '\n');
	Token tok = get(lex, TOK_NEWLINE);
	fwd(lex);
	++lex->lineno;
	return tok;
}

static Token eof_token(void)
{
	Token tok;
	tok.value = NULL;
	tok.length = 0;
	tok.type = TOK_EOF;
	tok.lineno = 0;
	return tok;
}

static void pass_comment(Lexer *lex)
{
	assert(currc(lex) == '#');
	while (currc(lex) != '\n' && nextc(lex) != EOF) {
		fwd(lex);
	}
}

static void add_token(Lexer *lex, Token *tok)
{
	const size_t size = lex->tok_count;
	const size_t capacity = lex->tok_capacity;

	if (size == capacity) {
		const size_t new_capacity = (capacity * 3) / 2 + 1;
		lex->tok_capacity = new_capacity;
		lex->tokens = rho_realloc(lex->tokens, new_capacity * sizeof(Token));
	}

	lex->tokens[lex->tok_count++] = *tok;
}

static void tokenize(Lexer *lex)
{
	while (true) {
		skip_spaces(lex);
		Token tok;
		const char c = lex->pos[0];

		if (c == '\0') {
			break;
		}

		if (isdigit(c)) {
			tok = next_number(lex);
		}
		else if (is_op_char(c)) {
			tok = next_op(lex);
		}
		else if (is_word_char(c)) {
			tok = next_word(lex);
		}
		else {
			switch (c) {
			case '(':
				tok = next_paren_open(lex);
				break;
			case ')':
				tok = next_paren_close(lex);
				break;
			case '{':
				tok = next_brace_open(lex);
				break;
			case '}':
				tok = next_brace_close(lex);
				break;
			case '[':
				tok = next_bracket_open(lex);
				break;
			case ']':
				tok = next_bracket_close(lex);
				break;
			case ',':
				tok = next_comma(lex);
				break;
			case '"':
			case '\'':
				tok = next_string(lex, c);
				break;
			case ':':
				tok = next_colon(lex);
				break;
			case ';':
				tok = next_semicolon(lex);
				break;
			case '\n':
				tok = next_newline(lex);
				break;
			case '#':
				pass_comment(lex);
				continue;
			default: {
				lex_err_unknown_char(lex, lex->pos);
				return;
			}
			}
		}

		add_token(lex, &tok);
	}

	Token eof = eof_token();
	eof.lineno = lex->lineno;
	add_token(lex, &eof);
}

Lexer *lex_new(char *str, const size_t length, const char *name)
{
	Lexer *lex = rho_malloc(sizeof(Lexer));
	lex->code = str;
	lex->end = &str[length - 1];
	lex->pos = &str[0];
	lex->mark = 0;
	lex->tokens = rho_malloc(INITIAL_TOKEN_ARRAY_CAPACITY * sizeof(Token));
	lex->tok_count = 0;
	lex->tok_capacity = INITIAL_TOKEN_ARRAY_CAPACITY;
	lex->tok_pos = 0;
	lex->lineno = 1;
	lex->peek = NULL;
	lex->name = name;
	lex->in_function = 0;
	lex->in_loop = 0;

	tokenize(lex);

	return lex;
}

/*
 * We don't care about certain tokens (e.g. newlines and
 * semicolons) except when they are required as statement
 * terminators. `lex_next_token` skips over these tokens,
 * but they can be accessed via `lex_next_token_direct`.
 *
 * `lex_peek_token` is analogous.
 */

Token *lex_next_token(Lexer *lex)
{
	Token *tok;
	do {
		tok = lex_next_token_direct(lex);
	} while (tok->type == TOK_NEWLINE);
	return tok;
}

Token *lex_next_token_direct(Lexer *lex)
{
	lex->peek = NULL;

	Token *next = &lex->tokens[lex->tok_pos];

	if (next->type != TOK_EOF) {
		++lex->tok_pos;
	}

	return next;
}

Token *lex_peek_token(Lexer *lex)
{
	if (lex->peek != NULL) {
		return lex->peek;
	}

	Token *tokens = lex->tokens;

	const size_t tok_count = lex->tok_count;
	const size_t tok_pos = lex->tok_pos;
	size_t offset = 0;

	while (tok_pos + offset < tok_count &&
	       tokens[tok_pos + offset].type == TOK_NEWLINE) {

		++offset;
	}

	while (tok_pos + offset >= tok_count) {
		--offset;
	}

	return lex->peek = &tokens[tok_pos + offset];
}

Token *lex_peek_token_direct(Lexer *lex)
{
	return &lex->tokens[lex->tok_pos];
}

bool lex_has_next(Lexer *lex)
{
	return lex->tokens[lex->tok_pos].type != TOK_EOF;
}

void lex_free(Lexer *lex)
{
	free(lex->tokens);
	free(lex);
}

static void lex_err_unknown_char(Lexer *lex, const char *c)
{
	fprintf(stderr,
	        SYNTAX_ERROR " unrecognized character: %c\n\n",
	        lex->name,
	        lex->lineno,
	        *c);

	err_on_char(c, lex->code, lex->end, lex->lineno);

	exit(EXIT_FAILURE);
}
