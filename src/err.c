#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "object.h"
#include "err.h"

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

void type_assert(Value *val, Class *type)
{
	bool fail = false;
	const char *name;
	switch (val->type) {
	case VAL_TYPE_EMPTY:
		fail = true;
		name = "<empty>";
		break;
	case VAL_TYPE_INT:
		// TODO: use int pseudo-class
		break;
	case VAL_TYPE_FLOAT:
		// TODO: use double pseudo-class
		break;
	case VAL_TYPE_OBJECT: {
		Object *o = val->data.o;
		name = o->class->name;
		fail = !instanceof(o, type);
		break;
	}
	}

	if (fail) {
		fprintf(stderr, "Type Error: expected %s, got %s", type->name, name);
		abort();
	}
}

void runtime_error(const char *type, const char *msg)
{
	fprintf(stderr, "%s: %s", type, msg);
	abort();
}

void fatal_error(const char *msg)
{
	fprintf(stderr, "fatal error: %s", msg);
	abort();
}

void unexpected_byte(const char *fn, const byte p)
{
	char buf[50];
	sprintf("unexpected byte in %s: %02x", fn, p);
	fatal_error(buf);
}

void internal_error(const char *fn, const unsigned int lineno)
{
	fprintf(stderr, "internal error in %s on line %d\n", fn, lineno);
	abort();
}
