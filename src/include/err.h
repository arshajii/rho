#ifndef RHO_ERR_H
#define RHO_ERR_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "code.h"
#include "object.h"

/*
 * Irrecoverable runtime-error handling facilities:
 */

#define RHO_ERR_TYPE_LIST \
	X(RHO_ERR_TYPE_FATAL,       "Fatal Error") \
	X(RHO_ERR_TYPE_TYPE,        "Type Error") \
	X(RHO_ERR_TYPE_NAME,        "Name Error") \
	X(RHO_ERR_TYPE_DIV_BY_ZERO, "Division by Zero Error") \
	X(RHO_ERR_TYPE_NO_MT,       "Multithreading Error")

#define X(a, b) a,
typedef enum {
	RHO_ERR_TYPE_LIST
} RhoErrorType;
#undef X

extern const char *rho_err_type_headers[];

struct rho_traceback_stack_item {
	const char *fn;
	unsigned int lineno;
};

struct rho_traceback_manager {
	struct rho_traceback_stack_item *tb;
	size_t tb_count;
	size_t tb_cap;
};

void rho_tb_manager_init(struct rho_traceback_manager *tbm);
void rho_tb_manager_add(struct rho_traceback_manager *tbm,
                        const char *fn,
                        const unsigned int lineno);
void rho_tb_manager_print(struct rho_traceback_manager *tbm, FILE *out);
void rho_tb_manager_dealloc(struct rho_traceback_manager *tbm);

typedef struct rho_error {
	RhoErrorType type;
	char msg[1024];
	struct rho_traceback_manager tbm;
} RhoError;

RhoError *rho_err_new(RhoErrorType type, const char *msg_format, ...);
void rho_err_free(RhoError *error);
void rho_err_traceback_append(RhoError *error,
                                const char *fn,
                                const unsigned int lineno);
void rho_err_traceback_print(RhoError *error, FILE *out);

RhoError *rho_err_invalid_file_signature_error(const char *module);
RhoError *rho_err_unbound(const char *var);
RhoError *rho_type_err_invalid_catch(const RhoClass *c1);
RhoError *rho_type_err_invalid_throw(const RhoClass *c1);
RhoError *rho_err_div_by_zero(void);
RhoError *rho_err_multithreading_not_supported(void);

void rho_err_print_msg(RhoError *e, FILE *out);

#define RHO_INTERNAL_ERROR() (assert(0))

/*
 * Syntax error message header. First format specifier
 * corresponds to the file, second corresponds to the
 * line number.
 */
#define RHO_SYNTAX_ERROR "%s:%d: syntax error:"

const char *rho_err_on_char(const char *culprit,
                            const char *code,
                            const char *end,
                            unsigned int target_line);

#endif /* RHO_ERR_H */
