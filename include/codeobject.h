#ifndef CODEOBJECT_H
#define CODEOBJECT_H

#include <stdbool.h>
#include "code.h"
#include "object.h"
#include "util.h"

struct rho_vm;

extern Class co_class;

typedef struct {
	Object base;

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
	struct str_array names;

	/* enumerated attributes used */
	struct str_array attrs;

	/* enumerated free variables */
	struct str_array frees;

	/* enumerated constants */
	struct value_array consts;

	/* line number table */
	byte *lno_table;

	/* first line number */
	unsigned int first_lineno;

	/* virtual machine associated with this code object */
	struct rho_vm *vm;
} CodeObject;

CodeObject *codeobj_make(Code *code,
                         const char *name,
                         unsigned int argcount,
                         int stack_depth,
                         int try_catch_depth,
                         struct rho_vm *vm);

#define CO_LOCALS_COUNT(co) ((co)->names.length)

#endif /* CODEOBJECT_H */
