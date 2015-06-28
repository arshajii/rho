#ifndef EXC_H
#define EXC_H

#include <stdbool.h>
#include "object.h"

typedef struct {
	Object base;
	const char *msg;
} Exception;

extern Class exception_class;

Value exc_make(Class *exc_class, bool active, const char *msg_format, ...);

#endif /* EXC_H */
