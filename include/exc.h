#ifndef EXC_H
#define EXC_H

#include <stdio.h>
#include <stdbool.h>
#include "err.h"
#include "object.h"

typedef struct {
	Object base;
	const char *msg;
	struct traceback_manager tbm;
} Exception;

typedef struct {
	Exception base;
} IndexException;

typedef struct {
	Exception base;
} TypeException;

typedef struct {
	Exception base;
} AttributeException;

typedef struct {
	Exception base;
} ImportException;

extern Class exception_class;
extern Class index_exception_class;
extern Class type_exception_class;
extern Class attr_exception_class;
extern Class import_exception_class;

Value exc_make(Class *exc_class, bool active, const char *msg_format, ...);
void exc_traceback_append(Exception *e,
                          const char *fn,
                          const unsigned int lineno);
void exc_traceback_print(Exception *e, FILE *out);
void exc_print_msg(Exception *e, FILE *out);

#define EXC(...)        exc_make(&exception_class, true, __VA_ARGS__)
#define INDEX_EXC(...)  exc_make(&index_exception_class, true, __VA_ARGS__)
#define TYPE_EXC(...)   exc_make(&type_exception_class, true, __VA_ARGS__)
#define ATTR_EXC(...)   exc_make(&attr_exception_class, true, __VA_ARGS__)
#define IMPORT_EXC(...) exc_make(&import_exception_class, true, __VA_ARGS__)

Value type_exc_unsupported_1(const char *op, const Class *c1);
Value type_exc_unsupported_2(const char *op, const Class *c1, const Class *c2);
Value type_exc_cannot_index(const Class *c1);
Value type_exc_cannot_apply(const Class *c1);
Value type_exc_cannot_instantiate(const Class *c1);
Value type_exc_not_callable(const Class *c1);
Value type_exc_not_iterable(const Class *c1);
Value type_exc_not_iterator(const Class *c1);
Value call_exc_num_args(const char *fn, unsigned int expected, unsigned int got);
Value call_exc_dup_arg(const char *fn, const char *name);
Value call_exc_unknown_arg(const char *fn, const char *name);
Value call_exc_missing_arg(const char *fn, const char *name);
Value call_exc_native_named_args(void);
Value call_exc_constructor_named_args(void);
Value attr_exc_not_found(const Class *type, const char *attr);
Value attr_exc_readonly(const Class *type, const char *attr);
Value attr_exc_mismatch(const Class *type, const char *attr, const Class *assign_type);
Value import_exc_not_found(const char *name);

#endif /* EXC_H */
