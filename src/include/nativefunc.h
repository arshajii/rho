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

#define RHO_NFUNC_INIT(func_) { .base = RHO_OBJ_INIT_STATIC(&rho_native_func_class), .func = (func_) }

#endif /* RHO_NATIVEFUNC_H */
