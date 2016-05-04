#ifndef RHO_NATIVEFUNC_H
#define RHO_NATIVEFUNC_H

#include "object.h"

/*
 * Note: NativeFuncObjects should be statically allocated only.
 */

extern RhoClass rho_native_func_class;

typedef RhoValue (*RhoNativeFunc)(RhoValue *args, size_t nargs);

typedef struct {
	RhoObject base;
	RhoNativeFunc func;
} RhoNativeFuncObject;

#endif /* RHO_NATIVEFUNC_H */
