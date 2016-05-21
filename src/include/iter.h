#ifndef RHO_ITER_H
#define RHO_ITER_H

#include "object.h"

/* Base iterator */
extern RhoClass rho_iter_class;

typedef struct {
	RhoObject base;
} RhoIter;

/* Singleton used to mark end of iteration */
extern RhoClass rho_iter_stop_class;

typedef struct {
	RhoObject base;
} RhoIterStop;

RhoValue rho_get_iter_stop(void);
#define rho_is_iter_stop(v) (rho_getclass((v)) == &rho_iter_stop_class)

/* Result of applying function to iterator */
extern RhoClass rho_applied_iter_class;

typedef struct {
	RhoIter base;
	RhoIter *source;
	RhoValue fn;
} RhoAppliedIter;

RhoValue rho_applied_iter_make(RhoIter *source, RhoValue *fn);

/* Range iterator */
extern RhoClass rho_range_class;

typedef struct {
	RhoIter base;
	long from;
	long to;
	long i;
} RhoRange;

RhoValue rho_range_make(RhoValue *from, RhoValue *to);

#endif /* RHO_ITER_H */
