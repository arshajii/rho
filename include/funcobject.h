#ifndef FUNCOBJECT_H
#define FUNCOBJECT_H

#include <stdlib.h>
#include "object.h"
#include "codeobject.h"

extern Class fn_class;

typedef struct {
	Object base;
	CodeObject *co;

	/* default arguments */
	struct value_array defaults;
} FuncObject;

Value funcobj_make(CodeObject *co);

void funcobj_init_defaults(FuncObject *co, Value *defaults, const size_t n_defaults);

#endif /* FUNCOBJECT_H */
