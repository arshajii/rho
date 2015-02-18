#ifndef NATIVEFUNC_H
#define NATIVEFUNC_H

#include "object.h"

/*
 * Note: NativeFuncObjects should be statically allocated only.
 */

extern Class native_func_class;

typedef Value (*NativeFunc)(Value *args, size_t nargs);

typedef struct {
	Object base;
	NativeFunc func;
} NativeFuncObject;

#endif /* NATIVEFUNC_H */
