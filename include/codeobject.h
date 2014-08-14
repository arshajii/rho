#ifndef CODEOBJECT_H
#define CODEOBJECT_H

#include "object.h"

extern Class co_class;

typedef struct {
	Object base;

	/* code segment */
	byte *bc;

	/* number of arguments */
	unsigned char argcount;

	/* enumerated variable names */
	struct str_array names;

	/* enumerated constants */
	struct value_array consts;
} CodeObject;

CodeObject *codeobj_make(Code *code);

#endif /* CODEOBJECT_H */
