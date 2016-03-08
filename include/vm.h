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

	/*
	 * Some opcodes could be using space on the stack
	 * long-term, so if we catch an exception, we don't
	 * want to clear the whole stack. This pointer
	 * defines where we should stop clearing.
	 */
	Value *purge_wall;
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
	byte *head;
	Frame *module;
	Frame *callstack;
	struct value_array globals;
	struct str_array global_names;
	StrDict exports;

	/*
	 * VM instances form a tree whose structure is
	 * determined by imports. This is a convenient way
	 * to retain the global variables of CodeObjects
	 * that are imported.
	 */
	struct rho_vm *children;
	struct rho_vm *sibling;
} VM;

VM *vm_new(void);
int vm_exec_code(VM *vm, Code *code);
void vm_pushframe(VM *vm, CodeObject *co);
void vm_eval_frame(VM *vm);
void vm_popframe(VM *vm);
void vm_free(VM *vm);

VM *get_current_vm(void);
void set_current_vm(VM *vm);

#endif /* VM_H */
