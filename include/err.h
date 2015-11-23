#ifndef ERR_H
#define ERR_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "code.h"
#include "object.h"

/*
 * Irrecoverable runtime-error handling facilities:
 */

#define ERR_TYPE_LIST \
	X(ERR_TYPE_FATAL,       "Fatal Error") \
	X(ERR_TYPE_TYPE,        "Type Error") \
	X(ERR_TYPE_NAME,        "Name Error") \
	X(ERR_TYPE_DIV_BY_ZERO, "Division by Zero Error")

#define X(a, b) a,
typedef enum {
	ERR_TYPE_LIST
} ErrorType;
#undef X

extern const char *err_type_headers[];

struct traceback_stack_item {
	const char *fn;
	const unsigned int lineno;
};

struct traceback_manager {
	struct traceback_stack_item *tb;
	size_t tb_count;
	size_t tb_cap;
};

void tb_manager_init(struct traceback_manager *tbm);
void tb_manager_add(struct traceback_manager *tbm,
                    const char *fn,
                    const unsigned int lineno);
void tb_manager_print(struct traceback_manager *tbm, FILE *out);
void tb_manager_dealloc(struct traceback_manager *tbm);

typedef struct error {
	ErrorType type;
	char msg[1024];
	struct traceback_manager tbm;
} Error;

Error *error_new(ErrorType type, const char *msg_format, ...);
void error_free(Error *error);
void error_traceback_append(Error *error,
                            const char *fn,
                            const unsigned int lineno);
void error_traceback_print(Error *error, FILE *out);

Error *invalid_file_signature_error(const char *module);
Error *unbound_error(const char *var);
Error *bad_load_global_error(const char *fn);
Error *type_error_invalid_cmp(const Class *c1);
Error *type_error_invalid_catch(const Class *c1);
Error *type_error_invalid_throw(const Class *c1);
Error *div_by_zero_error(void);

void error_print_msg(Error *e, FILE *out);

#define INTERNAL_ERROR() (assert(0))

/*
 * Syntax error message header. First format specifier
 * corresponds to the file, second corresponds to the
 * line number.
 */
#define SYNTAX_ERROR "%s:%d: syntax error:"

const char *err_on_char(const char *culprit,
                        const char *code,
                        const char *end,
                        unsigned int target_line);

#endif /* ERR_H */
