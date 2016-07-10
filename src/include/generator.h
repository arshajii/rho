#ifndef RHO_GENERATOR_H
#define RHO_GENERATOR_H

#include <stdlib.h>
#include "object.h"
#include "codeobject.h"
#include "vm.h"

extern RhoClass rho_gen_proxy_class;
extern RhoClass rho_gen_class;

typedef struct {
	RhoObject base;
	RhoCodeObject *co;
	struct rho_value_array defaults;
} RhoGeneratorProxy;

typedef struct {
	RhoObject base;
	RhoCodeObject *co;
	RhoFrame *frame;
	RHO_SAVED_TID_FIELD
} RhoGeneratorObject;

RhoValue rho_gen_proxy_make(RhoCodeObject *co);
RhoValue rho_gen_make(RhoGeneratorProxy *gp);
void rho_gen_proxy_init_defaults(RhoGeneratorProxy *gp, RhoValue *defaults, const size_t n_defaults);

#endif /* RHO_GENERATOR_H */
