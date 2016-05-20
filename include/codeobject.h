#ifndef RHO_CODEOBJECT_H
#define RHO_CODEOBJECT_H

#include <stdbool.h>
#include "code.h"
#include "object.h"
#include "str.h"

struct rho_vm;

extern RhoClass rho_co_class;

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
} RhoCodeObject;

RhoCodeObject *codeobj_make(RhoCode *code,
                            const char *name,
                            unsigned int argcount,
                            int stack_depth,
                            int try_catch_depth,
                            struct rho_vm *vm);

#define RHO_CO_LOCALS_COUNT(co) ((co)->names.length)

#endif /* RHO_CODEOBJECT_H */
