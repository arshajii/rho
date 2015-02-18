#ifndef STROBJECT_H
#define STROBJECT_H

#include <stdlib.h>
#include <stdbool.h>
#include "str.h"
#include "object.h"

extern struct num_methods str_num_methods;
extern struct seq_methods str_seq_methods;
extern Class str_class;

typedef struct {
	Object base;
	Str str;
	bool freeable;  /* whether the underlying buffer should be freed */
} StrObject;

Value strobj_make(Str value);
Value strobj_make_direct(const char *value, const size_t len);

#endif /* STROBJECT_H */
