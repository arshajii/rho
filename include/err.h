#ifndef ERR_H
#define ERR_H

#include <assert.h>
#include "code.h"
#include "object.h"

/*
 * Syntax error message header. First format specifier
 * corresponds to the file, second corresponds to the
 * line number.
 */
#define SYNTAX_ERROR "%s:%d: syntax error:"

void err_on_char(const char *culprit,
                 const char *code,
                 const char *end,
                 unsigned int target_line);

void unbound_error(const char *var);
void type_assert(Value *val, Class *type);
void type_error(const char *msg);
void type_error_unsupported_1(const char *op, const Class *c1);
void type_error_unsupported_2(const char *op, const Class *c1, const Class *c2);
void type_error_cannot_instantiate(const Class *c1);
void type_error_not_callable(const Class *c1);
void def_error_dup_params(const char *fn, const char *param_name);
void call_error_args(const char *fn, unsigned int expected, unsigned int got);
void attr_error_not_found(const Class *type, const char *attr);
void attr_error_readonly(const Class *type, const char *attr);
void attr_error_mismatch(const Class *type, const char *attr, const Class *assign_type);

void fatal_error(const char *msg);

void unexpected_byte(const char *fn, const byte p);
#define UNEXP_BYTE(p) (unexpected_byte(__FUNCTION__, (p)))

#define INTERNAL_ERROR() (assert(0))

#endif /* ERR_H */
