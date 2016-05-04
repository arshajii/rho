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

static void lex_err_unexpected_char(RhoParser *p, const char *c);

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
	case '@':
		return true;
	default:
		return false;
	}
}

static RhoTokType str_to_op_toktype(const char *str, const size_t len)
{
	if (len == 0)
		return RHO_TOK_NONE;

	switch (str[0]) {
	case '+':
		if (len == 1)
			return RHO_TOK_PLUS;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_ASSIGN_ADD;
			break;
		}
		break;
	case '-':
		if (len == 1)
			return RHO_TOK_MINUS;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_ASSIGN_SUB;
			break;
		}
		break;
	case '*':
		if (len == 1)
			return RHO_TOK_MUL;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_ASSIGN_MUL;
			break;
		case '*':
			if (len == 2)
				return RHO_TOK_POW;

			switch (str[2]) {
			case '=':
				if (len == 3)
					return RHO_TOK_ASSIGN_POW;
				break;
			}
			break;
		}
		break;
	case '/':
		if (len == 1)
			return RHO_TOK_DIV;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_ASSIGN_DIV;
			break;
		}
		break;
	case '%':
		if (len == 1)
			return RHO_TOK_MOD;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_ASSIGN_MOD;
			break;
		}
		break;
	case '&':
		if (len == 1)
			return RHO_TOK_BITAND;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_ASSIGN_BITAND;
			break;
		case '&':
			if (len == 2)
				return RHO_TOK_AND;
			break;
		}
		break;
	case '|':
		if (len == 1)
			return RHO_TOK_BITOR;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_ASSIGN_BITOR;
			break;
		case '|':
			if (len == 2)
				return RHO_TOK_OR;
			break;
		}
		break;
	case '^':
		if (len == 1)
			return RHO_TOK_XOR;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_ASSIGN_XOR;
			break;
		}
		break;
	case '!':
		if (len == 1)
			return RHO_TOK_NOT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_NOTEQ;
			break;
		}
		break;
	case '~':
		if (len == 1)
			return RHO_TOK_BITNOT;

		switch (str[1]) {
		}
		break;
	case '=':
		if (len == 1)
			return RHO_TOK_ASSIGN;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_EQUAL;
			break;
		}
		break;
	case '<':
		if (len == 1)
			return RHO_TOK_LT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_LE;
			break;
		case '<':
			if (len == 2)
				return RHO_TOK_SHIFTL;

			switch (str[2]) {
			case '=':
				if (len == 3)
					return RHO_TOK_ASSIGN_SHIFTL;
				break;
			}
			break;
		}
		break;
	case '>':
		if (len == 1)
			return RHO_TOK_GT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_GE;
			break;
		case '>':
			if (len == 2)
				return RHO_TOK_SHIFTR;

			switch (str[2]) {
			case '=':
				if (len == 3)
					return RHO_TOK_ASSIGN_SHIFTR;
				break;
			}
			break;
		}
		break;
	case '.':
		if (len == 1)
			return RHO_TOK_DOT;

		switch (str[1]) {
		case '.':
			if (len == 2)
				return RHO_TOK_DOTDOT;
			break;
		}
		break;
	case '@':
		if (len == 1)
			return RHO_TOK_AT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return RHO_TOK_ASSIGN_AT;
			break;
		}
		break;
	}

	return RHO_TOK_NONE;
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
static inline void fix(RhoParser *p)
{
	p->pos += p->mark + 1;
	p->mark = 0;
}

/*
 * Returns the lexer's current token based on
 * its position and mark.
 */
static RhoToken get(RhoParser *p, RhoTokType type)
{
	RhoToken tok;
	tok.value = p->pos;
	tok.length = p->mark + 1;
	tok.type = type;
	tok.lineno = p->lineno;

	return tok;
}

/*
 * Returns the lexer's current character.
 */
static inline char currc(RhoParser *p)
{
	return p->pos[p->mark];
}

/*
 * Returns the lexer's next character.
 */
static inline char nextc(RhoParser *p)
{
	return p->pos[p->mark + 1];
}

/*
 * Returns the lexer's next next character.
 */
static inline char next_nextc(RhoParser *p)
{
	return p->pos[p->mark + 2];
}

/*
 * Advances the lexer mark.
 */
static inline void adv(RhoParser *p)
{
	++p->mark;
}

/*
 * Forwards the lexer position.
 */
static inline void fwd(RhoParser *p)
{
	++p->pos;
}

/*
 * Rewinds the lexer mark.
 */
static inline void rew(RhoParser *p)
{
	--p->mark;
}

static inline bool isspace_except_newline(int c)
{
	return isspace(c) && c != '\n';
}

static void skip_spaces(RhoParser *p)
{
	while (isspace_except_newline(p->pos[0])) {
		fwd(p);
	}
}

static void read_digits(RhoParser *p)
{
	while (isdigit(nextc(p))) {
		adv(p);
	}
}

static RhoToken next_number(RhoParser *p)
{
	assert(isdigit(currc(p)));

	RhoTokType type = RHO_TOK_INT;
	read_digits(p);

	if (nextc(p) == '.' && !is_op_char(next_nextc(p))) {
		adv(p);
		read_digits(p);
		type = RHO_TOK_FLOAT;
	}

	RhoToken tok = get(p, type);
	fix(p);
	return tok;
}

static RhoToken next_string(RhoParser *p, const char delim)
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

	RhoToken tok = get(p, RHO_TOK_STR);

	fix(p);
	return tok;
}

static RhoToken next_op(RhoParser *p)
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
	RhoTokType optype;

	while ((optype = str_to_op_toktype(p->pos, p->mark + 1)) == RHO_TOK_NONE) {
		rew(p);
	}

	RhoToken tok = get(p, optype);
	fix(p);
	return tok;
}

static RhoToken next_word(RhoParser *p)
{
	static const struct {
		const char *keyword;
		RhoTokType type;
	} keywords[] = {
		{"print",    RHO_TOK_PRINT},
		{"if",       RHO_TOK_IF},
		{"elif",     RHO_TOK_ELIF},
		{"else",     RHO_TOK_ELSE},
		{"while",    RHO_TOK_WHILE},
		{"for",      RHO_TOK_FOR},
		{"in",       RHO_TOK_IN},
		{"def",      RHO_TOK_DEF},
		{"break",    RHO_TOK_BREAK},
		{"continue", RHO_TOK_CONTINUE},
		{"return",   RHO_TOK_RETURN},
		{"throw",    RHO_TOK_THROW},
		{"try",      RHO_TOK_TRY},
		{"catch",    RHO_TOK_CATCH},
		{"import",   RHO_TOK_IMPORT},
		{"export",   RHO_TOK_EXPORT}
	};

	assert(is_word_char(currc(p)));

	while (is_id_char(nextc(p))) {
		adv(p);
	}

	RhoToken tok = get(p, RHO_TOK_IDENT);
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

static RhoToken next_paren_open(RhoParser *p)
{
	assert(currc(p) == '(');
	RhoToken tok = get(p, RHO_TOK_PAREN_OPEN);
	fwd(p);
	return tok;
}

static RhoToken next_paren_close(RhoParser *p)
{
	assert(currc(p) == ')');
	RhoToken tok = get(p, RHO_TOK_PAREN_CLOSE);
	fwd(p);
	return tok;
}

static RhoToken next_brace_open(RhoParser *p)
{
	assert(currc(p) == '{');
	RhoToken tok = get(p, RHO_TOK_BRACE_OPEN);
	fwd(p);
	return tok;
}

static RhoToken next_brace_close(RhoParser *p)
{
	assert(currc(p) == '}');
	RhoToken tok = get(p, RHO_TOK_BRACE_CLOSE);
	fwd(p);
	return tok;
}

static RhoToken next_bracket_open(RhoParser *p)
{
	assert(currc(p) == '[');
	RhoToken tok = get(p, RHO_TOK_BRACK_OPEN);
	fwd(p);
	return tok;
}

static RhoToken next_bracket_close(RhoParser *p)
{
	assert(currc(p) == ']');
	RhoToken tok = get(p, RHO_TOK_BRACK_CLOSE);
	fwd(p);
	return tok;
}

static RhoToken next_comma(RhoParser *p)
{
	assert(currc(p) == ',');
	RhoToken tok = get(p, RHO_TOK_COMMA);
	fwd(p);
	return tok;
}

static RhoToken next_colon(RhoParser *p)
{
	assert(currc(p) == ':');
	RhoToken tok = get(p, RHO_TOK_COLON);
	fwd(p);
	return tok;
}

static RhoToken next_dollar_ident(RhoParser *p)
{
	assert(currc(p) == '$');

	if (!isdigit(nextc(p)) || nextc(p) == '0') {
		fwd(p);
		RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_UNEXPECTED_CHAR);
		static RhoToken x;
		return x;
	}

	read_digits(p);
	RhoToken tok = get(p, RHO_TOK_DOLLAR);
	fix(p);
	return tok;
}

static RhoToken next_semicolon(RhoParser *p)
{
	assert(currc(p) == ';');
	RhoToken tok = get(p, RHO_TOK_SEMICOLON);
	fwd(p);
	return tok;
}

static RhoToken next_newline(RhoParser *p)
{
	assert(currc(p) == '\n');
	RhoToken tok = get(p, RHO_TOK_NEWLINE);
	fwd(p);
	++p->lineno;
	return tok;
}

static RhoToken eof_token(void)
{
	RhoToken tok;
	tok.value = NULL;
	tok.length = 0;
	tok.type = RHO_TOK_EOF;
	tok.lineno = 0;
	return tok;
}

static void pass_comment(RhoParser *p)
{
	assert(currc(p) == '#');
	while (currc(p) != '\n' && currc(p) != '\0') {
		fwd(p);
	}
}

static void add_token(RhoParser *p, RhoToken *tok)
{
	const size_t size = p->tok_count;
	const size_t capacity = p->tok_capacity;

	if (size == capacity) {
		const size_t new_capacity = (capacity * 3) / 2 + 1;
		p->tok_capacity = new_capacity;
		p->tokens = rho_realloc(p->tokens, new_capacity * sizeof(RhoToken));
	}

	p->tokens[p->tok_count++] = *tok;
}

void rho_parser_tokenize(RhoParser *p)
{
	while (true) {
		skip_spaces(p);
		RhoToken tok;
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

		if (RHO_PARSER_ERROR(p)) {
			goto err;
		}

		add_token(p, &tok);
	}

	RhoToken eof = eof_token();
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

RhoToken *rho_parser_next_token(RhoParser *p)
{
	RhoToken *tok;
	do {
		tok = rho_parser_next_token_direct(p);
	} while (tok->type == RHO_TOK_NEWLINE);
	return tok;
}

RhoToken *rho_parser_next_token_direct(RhoParser *p)
{
	p->peek = NULL;

	RhoToken *next = &p->tokens[p->tok_pos];

	if (next->type != RHO_TOK_EOF) {
		++p->tok_pos;
	}

	return next;
}

RhoToken *rho_parser_peek_token(RhoParser *p)
{
	if (p->peek != NULL) {
		return p->peek;
	}

	RhoToken *tokens = p->tokens;

	const size_t tok_count = p->tok_count;
	const size_t tok_pos = p->tok_pos;
	size_t offset = 0;

	while (tok_pos + offset < tok_count &&
	       tokens[tok_pos + offset].type == RHO_TOK_NEWLINE) {

		++offset;
	}

	while (tok_pos + offset >= tok_count) {
		--offset;
	}

	return p->peek = &tokens[tok_pos + offset];
}

RhoToken *rho_parser_peek_token_direct(RhoParser *p)
{
	return &p->tokens[p->tok_pos];
}

bool rho_parser_has_next_token(RhoParser *p)
{
	return p->tokens[p->tok_pos].type != RHO_TOK_EOF;
}

static void lex_err_unexpected_char(RhoParser *p, const char *c)
{
	const char *tok_err = rho_err_on_char(c, p->code, p->end, p->lineno);
	RHO_PARSER_SET_ERROR_MSG(p,
	                         rho_util_str_format(RHO_SYNTAX_ERROR " unexpected character: %c\n\n%s",
	                            p->name, p->lineno, *c, tok_err));
	RHO_FREE(tok_err);
	RHO_PARSER_SET_ERROR_TYPE(p, RHO_PARSE_ERR_UNEXPECTED_CHAR);
}
