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

struct exc_stack_element {
	size_t start;  /* start position of try-block */
	size_t end;    /* end position of try-block */
	size_t handler_pos;  /* where to jump in case of exception */
};

typedef struct frame {
	CodeObject *co;
	Value *locals;
	Str *frees;  /* free variables */
	Value *valuestack;
	Value *valuestack_base;
	Value return_value;

	struct exc_stack_element *exc_stack_base;
	struct exc_stack_element *exc_stack;

	size_t pos;  /* position in bytecode */
	struct frame *prev;
} Frame;

typedef struct rho_vm {
	Frame *module;
	Frame *callstack;
	StrDict builtins;
	StrDict builtin_modules;
	StrDict exports;
	StrDict import_cache;
} VM;

VM *vm_new(void);
int vm_exec_code(VM *vm, Code *code);
void vm_pushframe(VM *vm, CodeObject *co);
void vm_eval_frame(VM *vm);
void vm_popframe(VM *vm);
void vm_free(VM *vm, bool dealloc_exports);

#endif /* VM_H */
