#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "intobject.h"
#include "floatobject.h"
#include "object.h"
#include "err.h"

#define TYPE_ERROR_HEADER "Type Error: "
#define NAME_ERROR_HEADER "Name Error: "
#define ATTR_ERROR_HEADER "Attribute Error: "
#define FATAL_ERROR_HEADER "Fatal Error: "

void err_on_char(const char *culprit,
                 const char *code,
                 const char *end,
                 unsigned int target_line)
{
#define MAX_LEN 1024

	char line_str[MAX_LEN];
	char mark_str[MAX_LEN];

	unsigned int lineno = 1;
	char *line = (char *)code;
	size_t line_len = 0;

	while (lineno != target_line) {
		if (*line == '\n') {
			++lineno;
		}

		++line;
	}

	while (line[line_len] != '\n' &&
	       line + line_len - 1 != end &&
	       line_len < MAX_LEN) {

		++line_len;
	}

	memcpy(line_str, line, line_len);
	line_str[line_len] = '\0';

	size_t tok_offset = 0;
	while (line + tok_offset != culprit && tok_offset < MAX_LEN) {
		++tok_offset;
	}

	memset(mark_str, ' ', tok_offset);
	mark_str[tok_offset] = '^';
	mark_str[tok_offset + 1] = '\0';

	for (size_t i = 0; i < tok_offset; i++) {
		if (line_str[i] == '\t') {
			mark_str[i] = '\t';
		}
	}

	fprintf(stderr,
			"%s\n%s\n",
	        line_str,
	        mark_str);

#undef MAX_LEN
}

void unbound_error(const char *var)
{
	fprintf(stderr,
	        NAME_ERROR_HEADER "cannot reference unbound variable %s",
	        var);

	exit(EXIT_FAILURE);
}

void type_assert(Value *val, Class *type)
{
	if (getclass(val) == type) {
		return;
	}

	fprintf(stderr,
	        TYPE_ERROR_HEADER "expected %s, got %s",
	        type->name,
	        getclass(val)->name);

	exit(EXIT_FAILURE);
}

void type_error(const char *msg)
{
	fprintf(stderr, TYPE_ERROR_HEADER "%s\n", msg);
	exit(EXIT_FAILURE);
}

void type_error_unsupported_1(const char *op, const Class *c1)
{
	fprintf(stderr,
	        TYPE_ERROR_HEADER "unsupported operand type for %s: %s\n",
	        op,
	        c1->name);

	exit(EXIT_FAILURE);
}

void type_error_unsupported_2(const char *op, const Class *c1, const Class *c2)
{
	fprintf(stderr,
	        TYPE_ERROR_HEADER "unsupported operand types for %s: %s and %s\n",
	        op,
	        c1->name,
	        c2->name);

	exit(EXIT_FAILURE);
}

void type_error_not_callable(const Class *c1)
{
	fprintf(stderr,
	        TYPE_ERROR_HEADER "object of type %s is not callable\n",
	        c1->name);

	exit(EXIT_FAILURE);
}

void call_error_args(const char *fn, int expected, int got)
{
	fprintf(stderr,
	        TYPE_ERROR_HEADER "function %s(): expected %d arguments, got %d\n",
	        fn,
	        expected,
	        got);

	exit(EXIT_FAILURE);
}

void attr_error(const Class *type, const char *attr)
{
	fprintf(stderr,
	        ATTR_ERROR_HEADER "object of type %s has no attribute '%s'\n",
	        type->name,
	        attr);

	exit(EXIT_FAILURE);
}

void fatal_error(const char *msg)
{
	fprintf(stderr, FATAL_ERROR_HEADER "%s", msg);
	exit(EXIT_FAILURE);
}

void unexpected_byte(const char *fn, const byte p)
{
	char buf[64];
	sprintf("unexpected byte in %s: %02x", fn, p);
	fatal_error(buf);
}

void internal_error(const char *fn, const unsigned int lineno)
{
	fprintf(stderr, "internal error in %s on line %d\n", fn, lineno);
	exit(EXIT_FAILURE);
}
