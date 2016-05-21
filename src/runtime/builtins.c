#include <stdlib.h>
#include "nativefunc.h"
#include "object.h"
#include "strobject.h"
#include "vmops.h"
#include "strdict.h"
#include "exc.h"
#include "module.h"
#include "builtins.h"

#define NFUNC_INIT(func_) { .base = RHO_OBJ_INIT_STATIC(&rho_native_func_class), .func = (func_) }

static RhoValue hash(RhoValue *args, size_t nargs);
static RhoValue str(RhoValue *args, size_t nargs);
static RhoValue len(RhoValue *args, size_t nargs);
static RhoValue iter(RhoValue *args, size_t nargs);
static RhoValue next(RhoValue *args, size_t nargs);
static RhoValue type(RhoValue *args, size_t nargs);

static RhoNativeFuncObject hash_nfo = NFUNC_INIT(hash);
static RhoNativeFuncObject str_nfo  = NFUNC_INIT(str);
static RhoNativeFuncObject len_nfo  = NFUNC_INIT(len);
static RhoNativeFuncObject iter_nfo = NFUNC_INIT(iter);
static RhoNativeFuncObject next_nfo = NFUNC_INIT(next);
static RhoNativeFuncObject type_nfo = NFUNC_INIT(type);

const struct rho_builtin rho_builtins[] = {
		{"hash", RHO_MAKE_OBJ(&hash_nfo)},
		{"str",  RHO_MAKE_OBJ(&str_nfo)},
		{"len",  RHO_MAKE_OBJ(&len_nfo)},
		{"iter", RHO_MAKE_OBJ(&iter_nfo)},
		{"next", RHO_MAKE_OBJ(&next_nfo)},
		{"type", RHO_MAKE_OBJ(&type_nfo)},
		{NULL,   RHO_MAKE_EMPTY()},
};

static RhoValue hash(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("hash", nargs, 1);
	return rho_op_hash(&args[0]);
}

static RhoValue str(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("str", nargs, 1);
	return rho_makeobj(rho_op_str(&args[0]));
}

static RhoValue len(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("len", nargs, 1);
	RhoClass *class = rho_getclass(&args[0]);
	LenFunc len = rho_resolve_len(class);

	if (!len) {
		return rho_type_exc_unsupported_1(__func__, class);
	}

	return rho_makeint(len(&args[0]));
}

static RhoValue iter(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("iter", nargs, 1);
	return rho_op_iter(args);
}

static RhoValue next(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("next", nargs, 1);
	return rho_op_iternext(args);
}

static RhoValue type(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("type", nargs, 1);
	return rho_makeobj(rho_getclass(&args[0]));
}

/* Built-in Modules */

/* math */
#include <math.h>

static RhoValue rho_cos(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("cos", nargs, 1);
	if (!(rho_isint(args) || rho_isfloat(args))) {
		return rho_type_exc_unsupported_1("cos", rho_getclass(args));
	}
	const double d = rho_isint(args) ? (double)rho_intvalue(args) : rho_floatvalue(args);
	return rho_makefloat(cos(d));
}

static RhoValue rho_sin(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("sin", nargs, 1);
	if (!(rho_isint(args) || rho_isfloat(args))) {
		return rho_type_exc_unsupported_1("sin", rho_getclass(args));
	}
	const double d = rho_isint(args) ? (double)rho_intvalue(args) : rho_floatvalue(args);
	return rho_makefloat(sin(d));
}

static RhoNativeFuncObject cos_nfo = NFUNC_INIT(rho_cos);
static RhoNativeFuncObject sin_nfo = NFUNC_INIT(rho_sin);

#define PI 3.14159265358979323846
#define E  2.71828182845904523536

const struct rho_builtin math_builtins[] = {
		{"pi",  RHO_MAKE_FLOAT(PI)},
		{"e",   RHO_MAKE_FLOAT(E)},
		{"cos", RHO_MAKE_OBJ(&cos_nfo)},
		{"sin", RHO_MAKE_OBJ(&sin_nfo)},
		{NULL,  RHO_MAKE_EMPTY()},
};

static void init_math_module(RhoBuiltInModule *mod)
{
	RhoStrDict *dict = &mod->base.contents;
	for (size_t i = 0; math_builtins[i].name != NULL; i++) {
		rho_strdict_put(dict, math_builtins[i].name, (RhoValue *)&math_builtins[i].value, false);
	}
}

RhoBuiltInModule rho_math_module = RHO_BUILTIN_MODULE_INIT_STATIC("math", init_math_module);

const RhoModule *rho_builtin_modules[] = {
		(RhoModule *)&rho_math_module,
		NULL
};
