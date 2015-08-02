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

#endif /* MODULE_H */
