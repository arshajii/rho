#ifndef RHO_MODULE_H
#define RHO_MODULE_H

#include "object.h"
#include "strdict.h"

extern RhoClass rho_module_class;

typedef struct {
	RhoObject base;
	const char *name;
	RhoStrDict contents;
} RhoModule;

RhoValue rho_module_make(const char *name, RhoStrDict *contents);

extern RhoClass rho_builtin_module_class;

typedef struct rho_built_in_module {
	RhoModule base;
	void (*init_func)(struct rho_built_in_module *mod);
	bool initialized;
} RhoBuiltInModule;

#define RHO_BUILTIN_MODULE_INIT_STATIC(name_, init_func_) { \
  .base = { .base = RHO_OBJ_INIT_STATIC(&rho_builtin_module_class), .name = (name_) }, \
  .init_func = (init_func_), \
  .initialized = false \
}

#endif /* RHO_MODULE_H */
