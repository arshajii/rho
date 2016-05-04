#ifndef RHO_BUILTINS_H
#define RHO_BUILTINS_H

#include "object.h"
#include "module.h"

struct rho_builtin {
	const char *name;
	RhoValue value;
};

/* rho_builtins[last].name == NULL */
extern const struct rho_builtin rho_builtins[];

/* rho_builtin_modules[last] == NULL */
extern const RhoModule *rho_builtin_modules[];

#endif /* RHO_BUILTINS_H */
