#ifndef ERR_H
#define ERR_H

#include <assert.h>
#include "code.h"
#include "object.h"

/*
 * Irrecoverable runtime-error handling facilities:
 */

#define ERR_TYPE_LIST \
	X(ERR_TYPE_TYPE,        "Type Error") \
	X(ERR_TYPE_ATTR,        "Attribute Error") \
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

Error *unbound_error(const char *var);
Error *type_error_unsupported_1(const char *op, const Class *c1);
Error *type_error_unsupported_2(const char *op, const Class *c1, const Class *c2);
Error *type_error_cannot_index(const Class *c1);
Error *type_error_cannot_instantiate(const Class *c1);
Error *type_error_not_callable(const Class *c1);
Error *type_error_invalid_cmp(const Class *c1);
Error *type_error_invalid_catch(const Class *c1);
Error *type_error_invalid_throw(const Class *c1);
Error *call_error_args(const char *fn, unsigned int expected, unsigned int got);
Error *attr_error_not_found(const Class *type, const char *attr);
Error *attr_error_readonly(const Class *type, const char *attr);
Error *attr_error_mismatch(const Class *type, const char *attr, const Class *assign_type);
Error *div_by_zero_error(void);

void fatal_error(const char *msg);

void unexpected_byte(const char *fn, const byte p);
#define UNEXP_BYTE(p) (unexpected_byte(__FUNCTION__, (p)))

#define INTERNAL_ERROR() (assert(0))

/*
 * Syntax error message header. First format specifier
 * corresponds to the file, second corresponds to the
 * line number.
 */
#define SYNTAX_ERROR "%s:%d: syntax error:"

void def_error_dup_params(const char *fn, const char *param_name);

void err_on_char(const char *culprit,
                 const char *code,
                 const char *end,
                 unsigned int target_line);

#endif /* ERR_H */
