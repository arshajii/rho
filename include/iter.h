#ifndef ITER_H
#define ITER_H

#include "object.h"

/* Base iterator */
extern Class iter_class;

typedef struct {
	Object base;
} Iter;

/* Singleton used to mark end of iteration */
extern Class iter_stop_class;

typedef struct {
	Object base;
} IterStop;

Value get_iter_stop(void);
#define is_iter_stop(v) (getclass((v)) == &iter_stop_class)

/* Result of applying function to iterator */
extern Class applied_iter_class;

typedef struct {
	Iter base;
	Iter *source;
	Value fn;
} AppliedIter;

Value applied_iter_make(Iter *source, Value *fn);

#endif /* ITER_H */
