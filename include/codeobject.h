#ifndef CODEOBJECT_H
#define CODEOBJECT_H

#include "code.h"
#include "object.h"
#include "util.h"

extern Class co_class;

typedef struct {
	Object base;

	/* name of this code object */
	const char *name;

	/* full code (including tables) */
	byte *head;

	/* code segment */
	byte *bc;

	/* number of arguments */
	unsigned int argcount;

	/* max value stack depth */
	unsigned int stack_depth;

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
} CodeObject;

CodeObject *codeobj_make(Code *code,
                         const char *name,
                         unsigned int argcount,
                         int stack_depth);

#endif /* CODEOBJECT_H */
