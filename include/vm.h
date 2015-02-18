#ifndef VM_H
#define VM_H

#include <stdlib.h>
#include <stdio.h>
#include "object.h"
#include "code.h"
#include "codeobject.h"
#include "str.h"
#include "strdict.h"
#include "util.h"

typedef struct frame {
	CodeObject *co;
	Value *locals;
	Str *frees;  /* free variables */
	Value *valuestack;
	Value return_value;
	size_t pos;  /* position in bytecode */
	struct frame *prev;
} Frame;

typedef struct {
	Frame *module;
	Frame *callstack;
	StrDict imports;
	StrDict builtins;
} VM;

VM *vm_new(void);
void vm_free(VM *vm);
void execute(FILE *compiled);

#endif /* VM_H */
