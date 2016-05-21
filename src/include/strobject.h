#ifndef RHO_STROBJECT_H
#define RHO_STROBJECT_H

#include <stdlib.h>
#include <stdbool.h>
#include "str.h"
#include "object.h"

extern struct rho_num_methods rho_str_num_methods;
extern struct rho_seq_methods rho_str_seq_methods;
extern RhoClass rho_str_class;

typedef struct rho_str_object {
	RhoObject base;
	RhoStr str;
	bool freeable;  /* whether the underlying buffer should be freed */
} RhoStrObject;

RhoValue rho_strobj_make(RhoStr value);
RhoValue rho_strobj_make_direct(const char *value, const size_t len);

#endif /* RHO_STROBJECT_H */
