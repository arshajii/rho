#ifndef ERR_H
#define ERR_H

#include "code.h"
#include "object.h"

#define RT_ERR_UNBOUND_VAR "Unbound variable"
#define RT_ERR_TYPE        "Type error"

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

void type_assert(Value *val, Class *type);

void runtime_error(const char *type, const char *msg);

void fatal_error(const char *msg);

void unexpected_byte(const char *fn, const byte p);
#define UNEXP_BYTE(p) (unexpected_byte(__FUNCTION__, (p)))

void internal_error(const char *fn, const unsigned int lineno);
#define INTERNAL_ERROR() (internal_error(__FUNCTION__, __LINE__))

#endif /* ERR_H */
