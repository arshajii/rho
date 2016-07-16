#ifndef RHO_VM_H
#define RHO_VM_H

#include <stdlib.h>
#include <stdio.h>
#include "object.h"
#include "code.h"
#include "codeobject.h"
#include "str.h"
#include "strdict.h"
#include "module.h"
#include "main.h"

struct rho_exc_stack_element {
	size_t start;  /* start position of try-block */
	size_t end;    /* end position of try-block */
	size_t handler_pos;  /* where to jump in case of exception */

	/*
	 * Some opcodes could be using space on the stack
	 * long-term, so if we catch an exception, we don't
	 * want to clear the whole stack. This pointer
	 * defines where we should stop clearing.
	 */
	RhoValue *purge_wall;
};

struct rho_mailbox;

#if RHO_THREADED
#include <stdatomic.h>
#endif

typedef struct rho_frame {
	RhoCodeObject *co;

	RhoValue *locals;
	size_t n_locals;

	RhoStr *frees;  /* free variables */
	RhoValue *val_stack;
	RhoValue *val_stack_base;
	RhoValue return_value;

	struct rho_exc_stack_element *exc_stack_base;
	struct rho_exc_stack_element *exc_stack;

	size_t pos;  /* position in bytecode */
	struct rho_frame *prev;

#if RHO_THREADED
	struct rho_mailbox *mailbox;  /* support for actors */
	atomic_flag owned;
#endif

	unsigned active            : 1;
	unsigned persistent        : 1;
	unsigned top_level         : 1;
	unsigned force_free_locals : 1;
} RhoFrame;

typedef struct rho_vm {
	byte *head;
	RhoFrame *module;
	RhoFrame *callstack;
	struct rho_value_array globals;
	struct rho_str_array global_names;
	RhoStrDict exports;

	/*
	 * VM instances form a tree whose structure is
	 * determined by imports. This is a convenient way
	 * to retain the global variables of CodeObjects
	 * that are imported.
	 */
	struct rho_vm *children;
	struct rho_vm *sibling;
} RhoVM;

RhoVM *rho_vm_new(void);
int rho_vm_exec_code(RhoVM *vm, RhoCode *code);
void rho_vm_push_frame(RhoVM *vm, RhoCodeObject *co);
void rho_vm_push_frame_direct(RhoVM *vm, RhoFrame *frame);
void rho_vm_eval_frame(RhoVM *vm);
void rho_vm_pop_frame(RhoVM *vm);
void rho_vm_free(RhoVM *vm);

RhoFrame *rho_frame_make(RhoCodeObject *co);
void rho_frame_save_state(RhoFrame *frame,
                          const size_t pos,
                          RhoValue ret_val,
                          RhoValue *val_stack,
                          struct rho_exc_stack_element *exc_stack);
void rho_frame_reset(RhoFrame *frame);
void rho_frame_free(RhoFrame *frame);

RhoVM *rho_current_vm_get(void);
void rho_current_vm_set(RhoVM *vm);

void rho_vm_register_module(const RhoModule *module);

#endif /* RHO_VM_H */
