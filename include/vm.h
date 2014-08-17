#ifndef VM_H
#define VM_H

#include <stdlib.h>
#include <stdio.h>
#include "object.h"
#include "code.h"
#include "codeobject.h"
#include "util.h"

#define DEFAULT_VSTACK_DEPTH 10

typedef struct frame {
	const char *name;
	CodeObject *co;

	Value *globals;
	Value *locals;

	Value *valuestack;
	size_t stack_capacity;

	Value return_value;
	struct frame *prev;
} Frame;

typedef struct {
	Frame *callstack;
} VM;

VM *vm_new(void);
void vm_free(VM *vm);
void execute(FILE *compiled);

#endif /* VM_H */
