#ifndef BUILTINS_H
#define BUILTINS_H

#include "object.h"
#include "module.h"

struct builtin {
	const char *name;
	Value value;
};

/* builtins[last].name == NULL */
extern const struct builtin builtins[];

/* builtin_modules[last] == NULL */
extern const Module *builtin_modules[];

#endif /* BUILTINS_H */
