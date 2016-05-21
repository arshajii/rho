#include <stdlib.h>
#include <math.h>
#include "object.h"
#include "nativefunc.h"
#include "exc.h"
#include "module.h"
#include "builtins.h"
#include "strdict.h"
#include "mathmodule.h"

static RhoValue rho_cos(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("cos", nargs, 1);
	if (!rho_isnumber(&args[0])) {
		return rho_type_exc_unsupported_1("cos", rho_getclass(&args[0]));
	}
	const double d = rho_floatvalue_force(&args[0]);
	return rho_makefloat(cos(d));
}

static RhoValue rho_sin(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("sin", nargs, 1);
	if (!rho_isnumber(&args[0])) {
		return rho_type_exc_unsupported_1("sin", rho_getclass(&args[0]));
	}
	const double d = rho_floatvalue_force(&args[0]);
	return rho_makefloat(sin(d));
}

static RhoNativeFuncObject cos_nfo = RHO_NFUNC_INIT(rho_cos);
static RhoNativeFuncObject sin_nfo = RHO_NFUNC_INIT(rho_sin);

#define PI 3.14159265358979323846
#define E  2.71828182845904523536

const struct rho_builtin math_builtins[] = {
		{"pi",  RHO_MAKE_FLOAT(PI)},
		{"e",   RHO_MAKE_FLOAT(E)},
		{"cos", RHO_MAKE_OBJ(&cos_nfo)},
		{"sin", RHO_MAKE_OBJ(&sin_nfo)},
		{NULL,  RHO_MAKE_EMPTY()},
};

RhoBuiltInModule rho_math_module = RHO_BUILTIN_MODULE_INIT_STATIC("math", &math_builtins[0]);
