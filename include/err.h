#ifndef ERR_H
#define ERR_H

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

typedef struct error {
	ErrorType type;
	char msg[1024];
} Error;

Error *error_new(ErrorType type, const char *msg_format, ...);

Error *invalid_file_signature_error(const char *module);
Error *unbound_error(const char *var);
Error *bad_load_global_error(const char *fn);
Error *type_error_invalid_cmp(const Class *c1);
Error *type_error_invalid_catch(const Class *c1);
Error *type_error_invalid_throw(const Class *c1);
Error *div_by_zero_error(void);

void fatal_error(const char *msg);

void unexpected_byte(const char *fn, const byte p);
#define UNEXP_BYTE(p) (unexpected_byte(__FUNCTION__, (p)))

#define INTERNAL_ERROR() (assert(0))
#define OUT_OF_MEM_ERROR() fatal_error("memory allocation failed -- out of memory")

/*
 * Syntax error message header. First format specifier
 * corresponds to the file, second corresponds to the
 * line number.
 */
#define SYNTAX_ERROR "%s:%d: syntax error:"

void def_error_dup_params(const char *fn, const char *param_name);

const char *err_on_char(const char *culprit,
                        const char *code,
                        const char *end,
                        unsigned int target_line);

#endif /* ERR_H */
