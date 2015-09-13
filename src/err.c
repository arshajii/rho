#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "intobject.h"
#include "floatobject.h"
#include "object.h"
#include "metaclass.h"
#include "util.h"
#include "exc.h"
#include "err.h"

#define X(a, b) b,
const char *err_type_headers[] = {
	ERR_TYPE_LIST
};
#undef X

#define TBM_INIT_CAPACITY 5
void tb_manager_init(struct traceback_manager *tbm)
{
	tbm->tb = rho_malloc(TBM_INIT_CAPACITY * sizeof(struct traceback_stack_item));
	tbm->tb_count = 0;
	tbm->tb_cap = TBM_INIT_CAPACITY;
}

void tb_manager_add(struct traceback_manager *tbm,
                    const char *fn,
                    const unsigned int lineno)
{
	const size_t cap = tbm->tb_cap;
	if (tbm->tb_count == cap) {
		const size_t new_cap = (cap * 3)/2 + 1;
		tbm->tb = rho_realloc(tbm->tb, new_cap * sizeof(struct traceback_stack_item));
		tbm->tb_cap = new_cap;
	}

	tbm->tb[tbm->tb_count++] = (struct traceback_stack_item){str_dup(fn), lineno};
}

void tb_manager_print(struct traceback_manager *tbm, FILE *out)
{
	fprintf(out, "Traceback:\n");
	const size_t count = tbm->tb_count;

	for (size_t i = 0; i < count; i++) {
		fprintf(out, "  Line %u in %s\n", tbm->tb[i].lineno, tbm->tb[i].fn);
	}
}

void tb_manager_dealloc(struct traceback_manager *tbm)
{
	const size_t count = tbm->tb_count;
	for (size_t i = 0; i < count; i++) {
		FREE(tbm->tb[i].fn);
	}
	free(tbm->tb);
}

Error *error_new(ErrorType type, const char *msg_format, ...)
{
	Error *error = rho_malloc(sizeof(Error));
	error->type = type;

	va_list args;
	va_start(args, msg_format);
	vsnprintf(error->msg, sizeof(error->msg), msg_format, args);
	va_end(args);

	tb_manager_init(&error->tbm);
	return error;
}

void error_free(Error *error)
{
	tb_manager_dealloc(&error->tbm);
	free(error);
}

void error_traceback_append(Error *error,
                            const char *fn,
                            const unsigned int lineno)
{
	tb_manager_add(&error->tbm, fn, lineno);
}

void error_traceback_print(Error *error, FILE *out)
{
	tb_manager_print(&error->tbm, out);
}

#define FUNC_ERROR_HEADER "Function Error: "
#define ATTR_ERROR_HEADER "Attribute Error: "
#define FATAL_ERROR_HEADER "Fatal Error: "

Error *invalid_file_signature_error(const char *module)
{
	return error_new(ERR_TYPE_FATAL,
	                 "invalid file signature encountered when loading module '%s'",
	                 module);
}

Error *unbound_error(const char *var)
{
	return error_new(ERR_TYPE_NAME, "cannot reference unbound variable '%s'", var);
}

Error *bad_load_global_error(const char *fn)
{
	return error_new(ERR_TYPE_NAME,
	                 "cannot load global variable from imported function %s",
	                 fn);
}

Error *type_error_invalid_cmp(const Class *c1)
{
	return error_new(ERR_TYPE_TYPE, "comparison of type '%s' did not return an int", c1->name);
}

Error *type_error_invalid_catch(const Class *c1)
{
	if (c1 == &meta_class) {
		return error_new(ERR_TYPE_TYPE,
		                 "cannot catch non-subclass of Exception");
	} else {
		return error_new(ERR_TYPE_TYPE,
		                 "cannot catch instances of class %s",
		                 c1->name);
	}
}

Error *type_error_invalid_throw(const Class *c1)
{
	return error_new(ERR_TYPE_TYPE,
	                 "can only throw instances of a subclass of Exception, not %s",
	                 c1->name);
}

Error *div_by_zero_error(void)
{
	return error_new(ERR_TYPE_DIV_BY_ZERO, "division or modulo by zero");
}

void error_print_msg(Error *e, FILE *out)
{
	fprintf(out, "%s: %s\n", err_type_headers[e->type], e->msg);
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

/* compilation errors */

const char *err_on_char(const char *culprit,
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

	return str_format("%s\n%s\n", line_str, mark_str);

#undef MAX_LEN
}

void def_error_dup_params(const char *fn, const char *param_name)
{
	fprintf(stderr,
	        FUNC_ERROR_HEADER "function %s has duplicate parameter name '%s'\n",
	        fn,
	        param_name);

	exit(EXIT_FAILURE);
}
