#ifndef METHOD_H
#define METHOD_H

#include "object.h"
#include "attr.h"

extern Class method_class;

typedef struct {
	Object base;
	Object *binder;
	MethodFunc method;
} Method;

Value methobj_make(Object *binder, MethodFunc meth_func);

#endif /* METHOD_H */
