#ifndef MODULE_H
#define MODULE_H

#include "object.h"
#include "strdict.h"

extern Class module_class;

typedef struct {
	Object base;
	const char *name;
	StrDict contents;
} Module;

Value module_make(const char *name, StrDict *contents);

extern Class builtin_module_class;

typedef struct built_in_module {
	Module base;
	void (*init_func)(struct built_in_module *mod);
	bool initialized;
} BuiltInModule;

#define BUILTIN_MODULE_INIT_STATIC(name_, init_func_) { \
  .base = { .base = OBJ_INIT_STATIC(&builtin_module_class), .name = (name_) }, \
  .init_func = (init_func_), \
  .initialized = false \
}

#endif /* MODULE_H */
