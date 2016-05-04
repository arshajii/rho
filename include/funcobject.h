#ifndef RHO_FUNCOBJECT_H
#define RHO_FUNCOBJECT_H

#include <stdlib.h>
#include "object.h"
#include "codeobject.h"

extern RhoClass rho_fn_class;

typedef struct {
	RhoObject base;
	RhoCodeObject *co;

	/* default arguments */
	struct rho_value_array defaults;
} RhoFuncObject;

RhoValue rho_funcobj_make(RhoCodeObject *co);

void rho_funcobj_init_defaults(RhoFuncObject *co, RhoValue *defaults, const size_t n_defaults);

#endif /* RHO_FUNCOBJECT_H */
