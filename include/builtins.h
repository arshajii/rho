#ifndef BUILTINS_H
#define BUILTINS_H

#include "object.h"

struct builtin {
	const char *name;
	Value value;
};

/* builtins[last].name == NULL */
extern const struct builtin builtins[];

#endif /* BUILTINS_H */
