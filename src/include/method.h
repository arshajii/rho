#ifndef RHO_METHOD_H
#define RHO_METHOD_H

#include "object.h"
#include "attr.h"

extern RhoClass rho_method_class;

typedef struct {
	RhoObject base;
	RhoValue binder;
	MethodFunc method;
} RhoMethod;

RhoValue rho_methobj_make(RhoValue *binder, MethodFunc meth_func);

#endif /* RHO_METHOD_H */
