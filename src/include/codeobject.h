#ifndef RHO_CODEOBJECT_H
#define RHO_CODEOBJECT_H

#include <stdbool.h>
#include "code.h"
#include "object.h"
#include "str.h"

struct rho_vm;
struct rho_frame;

extern RhoClass rho_co_class;

struct rho_code_cache {
	/* line number cache */
	unsigned int lineno;
};

typedef struct {
	RhoObject base;

	/* name of this code object */
	const char *name;

	/* code segment */
	byte *bc;

	/* number of arguments */
	unsigned int argcount;

	/* max value stack depth */
	unsigned int stack_depth;

	/* max try-catch depth */
	unsigned int try_catch_depth;

	/* enumerated variable names */
	struct rho_str_array names;

	/* enumerated attributes used */
	struct rho_str_array attrs;

	/* enumerated free variables */
	struct rho_str_array frees;

	/* enumerated constants */
	struct rho_value_array consts;

	/* line number table */
	byte *lno_table;

	/* first line number */
	unsigned int first_lineno;

	/* virtual machine associated with this code object */
	struct rho_vm *vm;

	/* caches */
	struct rho_frame *frame;
	struct rho_code_cache *cache;
} RhoCodeObject;

RhoCodeObject *rho_codeobj_make(RhoCode *code,
                                const char *name,
                                unsigned int argcount,
                                int stack_depth,
                                int try_catch_depth,
                                struct rho_vm *vm);

/*
 * The stack and try-catch depths must be read out of the given
 * code at the top level.
 */
RhoCodeObject *rho_codeobj_make_toplevel(RhoCode *code,
                                         const char *name,
                                         struct rho_vm *vm);

RhoValue rho_codeobj_load_args(RhoCodeObject *co,
                               struct rho_value_array *default_args,
                               RhoValue *args,
                               RhoValue *args_named,
                               size_t nargs,
                               size_t nargs_named,
                               RhoValue *locals);

#endif /* RHO_CODEOBJECT_H */
