#include <stdlib.h>
#include "object.h"
#include "strobject.h"
#include "fileobject.h"
#include "nativefunc.h"
#include "exc.h"
#include "module.h"
#include "builtins.h"
#include "strdict.h"
#include "iomodule.h"

static RhoValue open_file(RhoValue *args, size_t nargs)
{
#define NAME "open"

	RHO_ARG_COUNT_CHECK_BETWEEN(NAME, nargs, 1, 2);

	if (nargs == 2) {
		if (!rho_is_a(&args[0], &rho_str_class) || !rho_is_a(&args[0], &rho_str_class)) {
			return rho_type_exc_unsupported_2(NAME, rho_getclass(&args[0]), rho_getclass(&args[1]));
		}

		RhoStrObject *filename = rho_objvalue(&args[0]);
		RhoStrObject *mode = rho_objvalue(&args[1]);

		return rho_file_make(filename->str.value, mode->str.value);
	} else {
		if (!rho_is_a(&args[0], &rho_str_class)) {
			return rho_type_exc_unsupported_1(NAME, rho_getclass(&args[0]));
		}

		RhoStrObject *filename = rho_objvalue(&args[0]);

		return rho_file_make(filename->str.value, "r");
	}

#undef NAME
}

static RhoNativeFuncObject open_file_nfo = RHO_NFUNC_INIT(open_file);

const struct rho_builtin io_builtins[] = {
		{"open",  RHO_MAKE_OBJ(&open_file_nfo)},
		{NULL,  RHO_MAKE_EMPTY()},
};

RhoBuiltInModule rho_io_module = RHO_BUILTIN_MODULE_INIT_STATIC("io", &io_builtins[0]);
