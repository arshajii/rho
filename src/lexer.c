#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "util.h"
#include "err.h"
#include "parser.h"
#include "lexer.h"

/*
 * (Lexer data is stored within the Parser structure. The comments in
 * this source file use the term "lexer" to refer to the subset of the
 * Parser structure pertaining to lexical analysis.)
 *
 * The lexical analysis stage consists of splitting the input source
 * into tokens. The source is fully tokenized upon the creation of a
 * Parser instance, which can then be queried to retrieve the tokens in
 * succession. A "Lexer" can, therefore, be thought of as a simple queue
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
 * The Parser structure has a `pos` field and a `mark` field. When a
 * token is encountered, its first character is pointed to by `pos`,
 * and `mark` increases gradually from zero to "consume" the token.
 * Once the token has been read, `pos` is set to the start of the next
 * token and `mark` is set back to 0 so the same process can begin
 * again. This continues until tokenization is complete. Note that
 * `pos` is a character pointer and `mark` is an unsigned integer
 * indicating how many characters (starting from `pos`) have been
 * consumed as part of the token.
 */

static void lex_err_unexpected_char(Parser *p, const char *c);

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

/*
 * The next few functions have comments that use
 * terminology from the extended comment at the
 * start of this file.
 */

/*
 * Forwards the lexer to its current mark then
 * resets the mark.
 */
static inline void fix(Parser *p)
{
	p->pos += p->mark + 1;
	p->mark = 0;
}

/*
 * Returns the lexer's current token based on
 * its position and mark.
 */
static Token get(Parser *p, TokType type)
{
	Token tok;
	tok.value = p->pos;
	tok.length = p->mark + 1;
	tok.type = type;
	tok.lineno = p->lineno;

	return tok;
}

/*
 * Returns the lexer's current character.
 */
static inline char currc(Parser *p)
{
	return p->pos[p->mark];
}

/*
 * Returns the lexer's next character.
 */
static inline char nextc(Parser *p)
{
	return p->pos[p->mark + 1];
}

/*
 * Advances the lexer mark.
 */
static inline void adv(Parser *p)
{
	++p->mark;
}

/*
 * Forwards the lexer position.
 */
static inline void fwd(Parser *p)
{
	++p->pos;
}

/*
 * Rewinds the lexer mark.
 */
static inline void rew(Parser *p)
{
	--p->mark;
}

static inline bool isspace_except_newline(int c)
{
	return isspace(c) && c != '\n';
}

static void skip_spaces(Parser *p)
{
	while (isspace_except_newline(p->pos[0])) {
		fwd(p);
	}
}

static void read_digits(Parser *p)
{
	while (isdigit(nextc(p))) {
		adv(p);
	}
}

static Token next_number(Parser *p)
{
	assert(isdigit(currc(p)));

	TokType type = TOK_INT;
	read_digits(p);

	if (nextc(p) == '.') {
		adv(p);
		read_digits(p);
		type = TOK_FLOAT;
	}

	Token tok = get(p, type);
	fix(p);
	return tok;
}

static Token next_string(Parser *p, const char delim)
{
	assert(delim == '"' || delim == '\'');
	assert(currc(p) == delim);
	adv(p);  // skip the first quotation character

	unsigned int escape_count = 0;

	do {
		const char c = currc(p);

		if (c == '\\') {
			++escape_count;
		}
		else if (c == delim && escape_count % 2 == 0) {
			break;
		}
		else {
			escape_count = 0;
		}

		adv(p);
	} while (true);

	Token tok = get(p, TOK_STR);

	fix(p);
	return tok;
}

static Token next_op(Parser *p)
{
	assert(is_op_char(currc(p)));

	/*
	 * Consume all tokens that constitute simple operators,
	 * e.g. '+', '-', '*' etc.
	 */
	while (is_op_char(nextc(p))) {
		adv(p);
	}

	/*
	 * Back up until we reach a 'valid' operator,
	 * in accordance with the maximal munch rule.
	 */
	TokType optype;

	while ((optype = str_to_op_toktype(p->pos, p->mark + 1)) == TOK_NONE) {
		rew(p);
	}

	Token tok = get(p, optype);
	fix(p);
	return tok;
}

static Token next_word(Parser *p)
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
		{"for",      TOK_FOR},
		{"in",       TOK_IN},
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

	assert(is_word_char(currc(p)));

	while (is_id_char(nextc(p))) {
		adv(p);
	}

	Token tok = get(p, TOK_IDENT);
	fix(p);

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

static Token next_paren_open(Parser *p)
{
	assert(currc(p) == '(');
	Token tok = get(p, TOK_PAREN_OPEN);
	fwd(p);
	return tok;
}

static Token next_paren_close(Parser *p)
{
	assert(currc(p) == ')');
	Token tok = get(p, TOK_PAREN_CLOSE);
	fwd(p);
	return tok;
}

static Token next_brace_open(Parser *p)
{
	assert(currc(p) == '{');
	Token tok = get(p, TOK_BRACE_OPEN);
	fwd(p);
	return tok;
}

static Token next_brace_close(Parser *p)
{
	assert(currc(p) == '}');
	Token tok = get(p, TOK_BRACE_CLOSE);
	fwd(p);
	return tok;
}

static Token next_bracket_open(Parser *p)
{
	assert(currc(p) == '[');
	Token tok = get(p, TOK_BRACK_OPEN);
	fwd(p);
	return tok;
}

static Token next_bracket_close(Parser *p)
{
	assert(currc(p) == ']');
	Token tok = get(p, TOK_BRACK_CLOSE);
	fwd(p);
	return tok;
}

static Token next_comma(Parser *p)
{
	assert(currc(p) == ',');
	Token tok = get(p, TOK_COMMA);
	fwd(p);
	return tok;
}

static Token next_colon(Parser *p)
{
	assert(currc(p) == ':');
	Token tok = get(p, TOK_COLON);
	fwd(p);
	return tok;
}

static Token next_dollar_ident(Parser *p)
{
	assert(currc(p) == '$');

	if (!isdigit(nextc(p)) || nextc(p) == '0') {
		fwd(p);
		PARSER_SET_ERROR_TYPE(p, PARSE_ERR_UNEXPECTED_CHAR);
		Token x;
		return x;
	}

	read_digits(p);
	Token tok = get(p, TOK_DOLLAR);
	fix(p);
	return tok;
}

static Token next_semicolon(Parser *p)
{
	assert(currc(p) == ';');
	Token tok = get(p, TOK_SEMICOLON);
	fwd(p);
	return tok;
}

static Token next_newline(Parser *p)
{
	assert(currc(p) == '\n');
	Token tok = get(p, TOK_NEWLINE);
	fwd(p);
	++p->lineno;
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

static void pass_comment(Parser *p)
{
	assert(currc(p) == '#');
	while (currc(p) != '\n' && nextc(p) != EOF) {
		fwd(p);
	}
}

static void add_token(Parser *p, Token *tok)
{
	const size_t size = p->tok_count;
	const size_t capacity = p->tok_capacity;

	if (size == capacity) {
		const size_t new_capacity = (capacity * 3) / 2 + 1;
		p->tok_capacity = new_capacity;
		p->tokens = rho_realloc(p->tokens, new_capacity * sizeof(Token));
	}

	p->tokens[p->tok_count++] = *tok;
}

void parser_tokenize(Parser *p)
{
	while (true) {
		skip_spaces(p);
		Token tok;
		const char c = p->pos[0];

		if (c == '\0') {
			break;
		}

		if (isdigit(c)) {
			tok = next_number(p);
		}
		else if (is_op_char(c)) {
			tok = next_op(p);
		}
		else if (is_word_char(c)) {
			tok = next_word(p);
		}
		else {
			switch (c) {
			case '(':
				tok = next_paren_open(p);
				break;
			case ')':
				tok = next_paren_close(p);
				break;
			case '{':
				tok = next_brace_open(p);
				break;
			case '}':
				tok = next_brace_close(p);
				break;
			case '[':
				tok = next_bracket_open(p);
				break;
			case ']':
				tok = next_bracket_close(p);
				break;
			case ',':
				tok = next_comma(p);
				break;
			case '"':
			case '\'':
				tok = next_string(p, c);
				break;
			case ':':
				tok = next_colon(p);
				break;
			case '$':
				tok = next_dollar_ident(p);
				break;
			case ';':
				tok = next_semicolon(p);
				break;
			case '\n':
				tok = next_newline(p);
				break;
			case '#':
				pass_comment(p);
				continue;
			default:
				goto err;
			}
		}

		if (PARSER_ERROR(p)) {
			goto err;
		}

		add_token(p, &tok);
	}

	Token eof = eof_token();
	eof.lineno = p->lineno;
	add_token(p, &eof);
	return;

	err:
	lex_err_unexpected_char(p, p->pos);
	free(p->tokens);
	p->tokens = NULL;
	p->tok_count = 0;
	p->tok_capacity = 0;
}

/*
 * We don't care about certain tokens (e.g. newlines and
 * semicolons) except when they are required as statement
 * terminators. `parser_next_token` skips over these tokens,
 * but they can be accessed via `parser_next_token_direct`.
 *
 * `parser_peek_token` is analogous.
 */

Token *parser_next_token(Parser *p)
{
	Token *tok;
	do {
		tok = parser_next_token_direct(p);
	} while (tok->type == TOK_NEWLINE);
	return tok;
}

Token *parser_next_token_direct(Parser *p)
{
	p->peek = NULL;

	Token *next = &p->tokens[p->tok_pos];

	if (next->type != TOK_EOF) {
		++p->tok_pos;
	}

	return next;
}

Token *parser_peek_token(Parser *p)
{
	if (p->peek != NULL) {
		return p->peek;
	}

	Token *tokens = p->tokens;

	const size_t tok_count = p->tok_count;
	const size_t tok_pos = p->tok_pos;
	size_t offset = 0;

	while (tok_pos + offset < tok_count &&
	       tokens[tok_pos + offset].type == TOK_NEWLINE) {

		++offset;
	}

	while (tok_pos + offset >= tok_count) {
		--offset;
	}

	return p->peek = &tokens[tok_pos + offset];
}

Token *parser_peek_token_direct(Parser *p)
{
	return &p->tokens[p->tok_pos];
}

bool parser_has_next_token(Parser *p)
{
	return p->tokens[p->tok_pos].type != TOK_EOF;
}

static void lex_err_unexpected_char(Parser *p, const char *c)
{
	const char *tok_err = err_on_char(c, p->code, p->end, p->lineno);
	PARSER_SET_ERROR_MSG(p,
	                     str_format(SYNTAX_ERROR " unexpected character: %c\n\n%s",
	                                p->name, p->lineno, *c, tok_err));
	FREE(tok_err);
	PARSER_SET_ERROR_TYPE(p, PARSE_ERR_UNEXPECTED_CHAR);
}
