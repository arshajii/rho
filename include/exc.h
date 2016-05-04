#ifndef RHO_EXC_H
#define RHO_EXC_H

#include <stdio.h>
#include <stdbool.h>
#include "err.h"
#include "object.h"

typedef struct {
	RhoObject base;
	const char *msg;
	struct rho_traceback_manager tbm;
} RhoException;

typedef struct {
	RhoException base;
} RhoIndexException;

typedef struct {
	RhoException base;
} RhoTypeException;

typedef struct {
	RhoException base;
} RhoAttributeException;

typedef struct {
	RhoException base;
} RhoImportException;

extern RhoClass rho_exception_class;
extern RhoClass rho_index_exception_class;
extern RhoClass rho_type_exception_class;
extern RhoClass rho_attr_exception_class;
extern RhoClass rho_import_exception_class;

RhoValue rho_exc_make(RhoClass *exc_class, bool active, const char *msg_format, ...);
void rho_exc_traceback_append(RhoException *e,
                              const char *fn,
                              const unsigned int lineno);
void rho_exc_traceback_print(RhoException *e, FILE *out);
void rho_exc_print_msg(RhoException *e, FILE *out);

#define RHO_EXC(...)        rho_exc_make(&rho_exception_class, true, __VA_ARGS__)
#define RHO_INDEX_EXC(...)  rho_exc_make(&rho_index_exception_class, true, __VA_ARGS__)
#define RHO_TYPE_EXC(...)   rho_exc_make(&rho_type_exception_class, true, __VA_ARGS__)
#define RHO_ATTR_EXC(...)   rho_exc_make(&rho_attr_exception_class, true, __VA_ARGS__)
#define RHO_IMPORT_EXC(...) rho_exc_make(&rho_import_exception_class, true, __VA_ARGS__)

RhoValue rho_type_exc_unsupported_1(const char *op, const RhoClass *c1);
RhoValue rho_type_exc_unsupported_2(const char *op, const RhoClass *c1, const RhoClass *c2);
RhoValue rho_type_exc_cannot_index(const RhoClass *c1);
RhoValue rho_type_exc_cannot_apply(const RhoClass *c1);
RhoValue rho_type_exc_cannot_instantiate(const RhoClass *c1);
RhoValue rho_type_exc_not_callable(const RhoClass *c1);
RhoValue rho_type_exc_not_iterable(const RhoClass *c1);
RhoValue rho_type_exc_not_iterator(const RhoClass *c1);
RhoValue rho_call_exc_num_args(const char *fn, unsigned int expected, unsigned int got);
RhoValue rho_call_exc_dup_arg(const char *fn, const char *name);
RhoValue rho_call_exc_unknown_arg(const char *fn, const char *name);
RhoValue rho_call_exc_missing_arg(const char *fn, const char *name);
RhoValue rho_call_exc_native_named_args(void);
RhoValue rho_call_exc_constructor_named_args(void);
RhoValue rho_attr_exc_not_found(const RhoClass *type, const char *attr);
RhoValue rho_attr_exc_readonly(const RhoClass *type, const char *attr);
RhoValue rho_attr_exc_mismatch(const RhoClass *type, const char *attr, const RhoClass *assign_type);
RhoValue rho_import_exc_not_found(const char *name);

#endif /* RHO_EXC_H */
