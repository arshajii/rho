#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "compiler.h"
#include "opcodes.h"
#include "str.h"
#include "object.h"
#include "intobject.h"
#include "floatobject.h"
#include "strobject.h"
#include "listobject.h"
#include "tupleobject.h"
#include "setobject.h"
#include "dictobject.h"
#include "codeobject.h"
#include "funcobject.h"
#include "method.h"
#include "nativefunc.h"
#include "module.h"
#include "metaclass.h"
#include "null.h"
#include "attr.h"
#include "exc.h"
#include "err.h"
#include "code.h"
#include "compiler.h"
#include "builtins.h"
#include "loader.h"
#include "plugins.h"
#include "util.h"
#include "main.h"
#include "vmops.h"
#include "vm.h"

static RhoVM *current_vm = NULL;

RhoVM *rho_current_vm_get(void)
{
	return current_vm;
}

void rho_current_vm_set(RhoVM *vm)
{
	current_vm = vm;
}

static RhoClass *classes[] = {
	&rho_obj_class,
	&rho_null_class,
	&rho_int_class,
	&rho_float_class,
	&rho_str_class,
	&rho_list_class,
	&rho_tuple_class,
	&rho_set_class,
	&rho_dict_class,
	&rho_co_class,
	&rho_fn_class,
	&rho_method_class,
	&rho_native_func_class,
	&rho_module_class,
	&rho_meta_class,

	/* exception classes */
	&rho_exception_class,
	&rho_index_exception_class,
	&rho_type_exception_class,
	&rho_attr_exception_class,
	&rho_import_exception_class,
	NULL
};

static RhoStrDict builtins_dict;
static RhoStrDict builtin_modules_dict;
static RhoStrDict import_cache;

static void builtins_dict_dealloc(void)
{
	rho_strdict_dealloc(&builtins_dict);
}

static void builtin_modules_dict_dealloc(void)
{
	rho_strdict_dealloc(&builtin_modules_dict);
}

static void builtin_modules_dealloc(void)
{
	for (size_t i = 0; rho_builtin_modules[i] != NULL; i++) {
		rho_strdict_dealloc((RhoStrDict *)&rho_builtin_modules[i]->contents);
	}
}

static void import_cache_dealloc(void)
{
	rho_strdict_dealloc(&import_cache);
}

static unsigned int get_lineno(RhoFrame *frame);

static void vm_push_module_frame(RhoVM *vm, RhoCode *code);
static void vm_load_builtins(void);
static void vm_load_builtin_modules(void);
static RhoValue vm_import(RhoVM *vm, const char *name);

RhoVM *rho_vm_new(void)
{
	static bool init = false;

	if (!init) {
		for (RhoClass **class = &classes[0]; *class != NULL; class++) {
			rho_class_init(*class);
		}

		rho_strdict_init(&builtins_dict);
		rho_strdict_init(&builtin_modules_dict);
		rho_strdict_init(&import_cache);

		vm_load_builtins();
		vm_load_builtin_modules();

		atexit(builtins_dict_dealloc);
		atexit(builtin_modules_dict_dealloc);
		atexit(builtin_modules_dealloc);
		atexit(import_cache_dealloc);

#if RHO_IS_POSIX
		const char *path = getenv(RHO_PLUGIN_PATH_ENV);
		if (path != NULL) {
			rho_set_plugin_path(path);
			if (rho_reload_plugins() != 0) {
				fprintf(stderr, RHO_WARNING_HEADER "could not load plug-ins at %s\n", path);
			}
		}
#endif

		init = true;
	}

	RhoVM *vm = rho_malloc(sizeof(RhoVM));
	vm->head = NULL;
	vm->module = NULL;
	vm->callstack = NULL;
	vm->globals = (struct rho_value_array){.array = NULL, .length = 0};
	vm->lno_cache = NULL;
	vm->children = NULL;
	vm->sibling = NULL;
	rho_strdict_init(&vm->exports);
	return vm;
}

static void vm_free_helper(RhoVM *vm)
{
	free(vm->head);
	const size_t n_globals = vm->globals.length;
	RhoValue *globals = vm->globals.array;

	for (size_t i = 0; i < n_globals; i++) {
		rho_release(&globals[i]);
	}

	free(globals);
	free(vm->global_names.array);

	for (RhoVM *child = vm->children; child != NULL;) {
		RhoVM *temp = child;
		child = child->sibling;
		vm_free_helper(temp);
	}

	free(vm->lno_cache);
	free(vm);
}

void rho_vm_free(RhoVM *vm)
{
	/*
	 * We only deallocate the export dictionary of the
	 * top-level VM, since those of child VMs (i.e. the
	 * result of imports) will be deallocated when their
	 * associated Module instances are deallocated.
	 */
	rho_strdict_dealloc(&vm->exports);
	vm_free_helper(vm);
}

static void vm_link(RhoVM *parent, RhoVM *child)
{
	child->sibling = parent->children;
	parent->children = child;
}

int rho_vm_exec_code(RhoVM *vm, RhoCode *code)
{
	vm->head = code->bc;

	free(vm->lno_cache);
	vm->lno_cache = rho_calloc(code->size, sizeof(unsigned int));
	vm_push_module_frame(vm, code);
	rho_vm_eval_frame(vm);

	RhoValue *ret = &vm->callstack->return_value;

	int status = 0;
	if (rho_isexc(ret)) {
		status = 1;
		RhoException *e = (RhoException *)rho_objvalue(ret);
		rho_exc_traceback_print(e, stderr);
		rho_exc_print_msg(e, stderr);
		rho_release(ret);
	} else if (rho_iserror(ret)) {
		status = 1;
		Error *e = rho_errvalue(ret);
		rho_err_traceback_print(e, stderr);
		rho_err_print_msg(e, stderr);
		rho_err_free(e);
	}

	rho_vm_popframe(vm);
	return status;
}

void rho_vm_pushframe(RhoVM *vm, RhoCodeObject *co)
{
	RhoFrame *frame = rho_malloc(sizeof(RhoFrame));
	frame->co = co;

	const size_t n_locals = co->names.length;
	const size_t stack_depth = co->stack_depth;
	const size_t try_catch_depth = co->try_catch_depth;

	frame->locals = rho_calloc(n_locals + stack_depth, sizeof(RhoValue));
	frame->valuestack = frame->valuestack_base = frame->locals + n_locals;

	const size_t frees_len = co->frees.length;
	RhoStr *frees = rho_malloc(frees_len * sizeof(RhoStr));
	for (size_t i = 0; i < frees_len; i++) {
		frees[i] = RHO_STR_INIT(co->frees.array[i].str, co->frees.array[i].length, 0);
	}
	frame->frees = frees;

	frame->exc_stack_base = frame->exc_stack = rho_malloc(try_catch_depth * sizeof(struct rho_exc_stack_element));

	frame->pos = 0;
	frame->prev = vm->callstack;
	frame->return_value = rho_makeempty();
	vm->callstack = frame;
}

void rho_vm_popframe(RhoVM *vm)
{
	RhoFrame *frame = vm->callstack;
	vm->callstack = frame->prev;

	/*
	 * We don't free the module-level local variables,
	 * because these are actually global variables and
	 * may still be referred to via imports. These will
	 * eventually be freed once the VM instance itself
	 * is freed.
	 */
	if (frame != vm->module) {
		const size_t n_locals = RHO_CO_LOCALS_COUNT(frame->co);
		RhoValue *locals = frame->locals;

		for (size_t i = 0; i < n_locals; i++) {
			rho_release(&locals[i]);
		}

		free(locals);
	}

	free(frame->frees);
	free(frame->exc_stack_base);
	rho_releaseo(frame->co);
	rho_release(&frame->return_value);
	free(frame);

	/*
	 * It's the frame evaluation function's job to make
	 * sure nothing is on the value stack before the frame
	 * is popped.
	 */
}

/*
 * Assumes the symbol table and constant table have not yet been read.
 */
static void vm_push_module_frame(RhoVM *vm, RhoCode *code)
{
	assert(vm->module == NULL);
	RhoCodeObject *co = codeobj_make(code, "<module>", 0, -1, -1, vm);
	rho_vm_pushframe(vm, co);
	vm->module = vm->callstack;
	vm->globals = (struct rho_value_array){.array = vm->module->locals,
	                                   .length = RHO_CO_LOCALS_COUNT(co)};
	rho_util_str_array_dup(&co->names, &vm->global_names);
}

void rho_vm_eval_frame(RhoVM *vm)
{
#define GET_BYTE()    (bc[pos++])
#define GET_UINT16()  (pos += 2, ((bc[pos - 1] << 8) | bc[pos - 2]))

#define IN_TOP_FRAME()  (vm->callstack == vm->module)

#define STACK_POP()          (--stack)
#define STACK_POPN(n)        (stack -= (n))
#define STACK_TOP()          (&stack[-1])
#define STACK_SECOND()       (&stack[-2])
#define STACK_THIRD()        (&stack[-3])
#define STACK_PUSH(v)        (*stack++ = (v))
#define STACK_SET_TOP(v)     (stack[-1] = (v))
#define STACK_SET_SECOND(v)  (stack[-2] = (v))
#define STACK_SET_THIRD(v)   (stack[-3] = (v))
#define STACK_PURGE(wall)    do { while (stack != wall) {rho_release(STACK_POP());} } while (0)

#define EXC_STACK_PUSH(start, end, handler, purge_wall) \
	(*exc_stack++ = (struct rho_exc_stack_element){(start), (end), (handler), (purge_wall)})
#define EXC_STACK_POP()       (--exc_stack)
#define EXC_STACK_TOP()       (&exc_stack[-1])
#define EXC_STACK_EMPTY()     (exc_stack == exc_stack_base)

	RhoFrame *frame = vm->callstack;

	RhoValue *locals = frame->locals;
	RhoStr *frees = frame->frees;
	const RhoCodeObject *co = frame->co;
	const RhoVM *co_vm = co->vm;
	RhoValue *globals = co_vm->globals.array;

	struct rho_str_array symbols = co->names;
	struct rho_str_array attrs = co->attrs;
	struct rho_str_array global_symbols = co_vm->global_names;

	RhoValue *constants = co->consts.array;
	byte *bc = co->bc;
	RhoValue *stack_base = frame->valuestack_base;
	RhoValue *stack = frame->valuestack;

	struct rho_exc_stack_element *exc_stack_base = frame->exc_stack_base;
	struct rho_exc_stack_element *exc_stack = frame->exc_stack;

	/* position in the bytecode */
	size_t pos = 0;

	RhoValue *v1, *v2, *v3;
	RhoValue res;

	head:
	while (true) {
		frame->pos = pos;

		while (!EXC_STACK_EMPTY() && (pos < EXC_STACK_TOP()->start || pos > EXC_STACK_TOP()->end)) {
			EXC_STACK_POP();
		}

		const byte opcode = GET_BYTE();

		switch (opcode) {
		case RHO_INS_NOP:
			break;
		case RHO_INS_LOAD_CONST: {
			const unsigned int id = GET_UINT16();
			v1 = &constants[id];
			rho_retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case RHO_INS_LOAD_NULL: {
			STACK_PUSH(rho_makenull());
			break;
		}
		/*
		 * Q: Why is the error check sandwiched between releasing v2 and
		 *    releasing v1?
		 *
		 * A: We want to use TOP/SET_TOP instead of POP/PUSH when we can,
		 *    and doing so in the case of binary operators means v1 will
		 *    still be on the stack while v2 will have been popped as we
		 *    perform the operation (e.g. op_add). Since the error section
		 *    purges the stack (which entails releasing everything on it),
		 *    releasing v1 before the error check would lead to an invalid
		 *    double-release of v1 in the case of an error/exception.
		 */
		case RHO_INS_ADD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_add(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_SUB: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_sub(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_MUL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_mul(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_DIV: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_div(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_MOD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_mod(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_POW: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_pow(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_BITAND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_bitand(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_BITOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_bitor(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_XOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_xor(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_BITNOT: {
			v1 = STACK_TOP();
			res = rho_op_bitnot(v1);

			if (rho_iserror(&res)) {
				goto error;
			}

			rho_release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_SHIFTL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_shiftl(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_SHIFTR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_shiftr(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_AND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_and(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_OR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_or(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_NOT: {
			v1 = STACK_TOP();
			res = rho_op_not(v1);

			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_EQUAL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_eq(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_NOTEQ: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_neq(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_LT: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_lt(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_GT: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_gt(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_LE: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_le(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_GE: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_ge(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_UPLUS: {
			v1 = STACK_TOP();
			res = rho_op_plus(v1);

			if (rho_iserror(&res)) {
				goto error;
			}

			rho_release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_UMINUS: {
			v1 = STACK_TOP();
			res = rho_op_minus(v1);

			if (rho_iserror(&res)) {
				goto error;
			}

			rho_release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_IADD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_iadd(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_ISUB: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_isub(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_IMUL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_imul(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_IDIV: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_idiv(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_IMOD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_imod(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_IPOW: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_ipow(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_IBITAND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_ibitand(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_IBITOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_ibitor(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_IXOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_ixor(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_ISHIFTL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_ishiftl(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_ISHIFTR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_ishiftr(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_MAKE_RANGE: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_range_make(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_IN: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_in(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_STORE: {
			v1 = STACK_POP();
			const unsigned int id = GET_UINT16();
			RhoValue old = locals[id];
			locals[id] = *v1;
			rho_release(&old);
			break;
		}
		case RHO_INS_STORE_GLOBAL: {
			v1 = STACK_POP();
			const unsigned int id = GET_UINT16();
			RhoValue old = globals[id];
			globals[id] = *v1;
			rho_release(&old);
			break;
		}
		case RHO_INS_LOAD: {
			const unsigned int id = GET_UINT16();
			v1 = &locals[id];

			if (rho_isempty(v1)) {
				res = rho_makeerr(rho_err_unbound(symbols.array[id].str));
				goto error;
			}

			rho_retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case RHO_INS_LOAD_GLOBAL: {
			const unsigned int id = GET_UINT16();
			v1 = &globals[id];

			if (rho_isempty(v1)) {
				res = rho_makeerr(rho_err_unbound(global_symbols.array[id].str));
				goto error;
			}

			rho_retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case RHO_INS_LOAD_ATTR: {
			v1 = STACK_TOP();
			const unsigned int id = GET_UINT16();
			const char *attr = attrs.array[id].str;
			res = rho_op_get_attr(v1, attr);

			if (rho_iserror(&res)) {
				goto error;
			}

			rho_release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_SET_ATTR: {
			v1 = STACK_POP();
			v2 = STACK_POP();
			const unsigned int id = GET_UINT16();
			const char *attr = attrs.array[id].str;
			res = rho_op_set_attr(v1, attr, v2);

			rho_release(v1);
			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			break;
		}
		case RHO_INS_LOAD_INDEX: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_get(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_SET_INDEX: {
			/* X[N] = Y */
			v3 = STACK_POP();  /* N */
			v2 = STACK_POP();  /* X */
			v1 = STACK_POP();  /* Y */

			res = rho_op_set(v2, v3, v1);

			rho_release(v1);
			rho_release(v2);
			rho_release(v3);

			if (rho_iserror(&res)) {
				goto error;
			}

			rho_release(&res);
			break;
		}
		case RHO_INS_APPLY: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_apply(v2, v1);  // yes, the arguments are reversed

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}
			rho_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_IAPPLY: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = rho_op_iapply(v1, v2);

			rho_release(v2);
			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_LOAD_NAME: {
			const unsigned int id = GET_UINT16();
			RhoStr *key = &frees[id];
			res = rho_strdict_get(&builtins_dict, key);

			if (!rho_isempty(&res)) {
				rho_retain(&res);
				STACK_PUSH(res);
				break;
			}

			res = rho_makeerr(rho_err_unbound(key->value));
			goto error;
			break;
		}
		case RHO_INS_PRINT: {
			v1 = STACK_POP();
			rho_op_print(v1, stdout);
			rho_release(v1);
			break;
		}
		case RHO_INS_JMP: {
			const unsigned int jmp = GET_UINT16();
			pos += jmp;
			break;
		}
		case RHO_INS_JMP_BACK: {
			const unsigned int jmp = GET_UINT16();
			pos -= jmp;
			break;
		}
		case RHO_INS_JMP_IF_TRUE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (rho_resolve_nonzero(rho_getclass(v1))(v1)) {
				pos += jmp;
			}
			rho_release(v1);
			break;
		}
		case RHO_INS_JMP_IF_FALSE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (!rho_resolve_nonzero(rho_getclass(v1))(v1)) {
				pos += jmp;
			}
			rho_release(v1);
			break;
		}
		case RHO_INS_JMP_BACK_IF_TRUE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (rho_resolve_nonzero(rho_getclass(v1))(v1)) {
				pos -= jmp;
			}
			rho_release(v1);
			break;
		}
		case RHO_INS_JMP_BACK_IF_FALSE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (!rho_resolve_nonzero(rho_getclass(v1))(v1)) {
				pos -= jmp;
			}
			rho_release(v1);
			break;
		}
		case RHO_INS_JMP_IF_TRUE_ELSE_POP: {
			v1 = STACK_TOP();
			const unsigned int jmp = GET_UINT16();
			if (rho_resolve_nonzero(rho_getclass(v1))(v1)) {
				pos += jmp;
			} else {
				STACK_POP();
				rho_release(v1);
			}
			break;
		}
		case RHO_INS_JMP_IF_FALSE_ELSE_POP: {
			v1 = STACK_TOP();
			const unsigned int jmp = GET_UINT16();
			if (!rho_resolve_nonzero(rho_getclass(v1))(v1)) {
				pos += jmp;
			} else {
				STACK_POP();
				rho_release(v1);
			}
			break;
		}
		case RHO_INS_CALL: {
			const unsigned int x = GET_UINT16();
			const unsigned int nargs = (x & 0xff);
			const unsigned int nargs_named = (x >> 8);
			v1 = STACK_POP();
			res = rho_op_call(v1,
			              stack - nargs_named*2 - nargs,
			              stack - nargs_named*2,
						  nargs,
						  nargs_named);

			rho_release(v1);
			if (rho_iserror(&res)) {
				goto error;
			}

			for (unsigned int i = 0; i < nargs_named; i++) {
				rho_release(STACK_POP());  // value
				rho_release(STACK_POP());  // name
			}

			for (unsigned int i = 0; i < nargs; i++) {
				rho_release(STACK_POP());
			}

			STACK_PUSH(res);
			break;
		}
		case RHO_INS_RETURN: {
			v1 = STACK_POP();
			rho_retain(v1);
			frame->return_value = *v1;
			STACK_PURGE(stack_base);
			return;
		}
		case RHO_INS_THROW: {
			v1 = STACK_POP();  // exception
			RhoClass *class = rho_getclass(v1);

			if (!rho_is_subclass(class, &rho_exception_class)) {
				res = rho_makeerr(rho_type_err_invalid_throw(class));
				goto error;
			}

			res = *v1;
			res.type = RHO_VAL_TYPE_EXC;
			goto error;
		}
		case RHO_INS_TRY_BEGIN: {
			const unsigned int try_block_len = GET_UINT16();
			const unsigned int handler_offset = GET_UINT16();

			EXC_STACK_PUSH(pos, pos + try_block_len, pos + handler_offset, stack);

			break;
		}
		case RHO_INS_TRY_END: {
			EXC_STACK_POP();
			break;
		}
		case RHO_INS_JMP_IF_EXC_MISMATCH: {
			const unsigned int jmp = GET_UINT16();

			v1 = STACK_POP();  // exception type
			v2 = STACK_POP();  // exception

			RhoClass *class = rho_getclass(v1);

			if (class != &rho_meta_class) {
				res = rho_makeerr(rho_type_err_invalid_catch(class));
				goto error;
			}

			RhoClass *exc_type = (RhoClass *)rho_objvalue(v1);

			if (!rho_is_a(v2, exc_type)) {
				pos += jmp;
			}

			rho_release(v1);
			rho_release(v2);

			break;
		}
		case RHO_INS_MAKE_LIST: {
			const unsigned int len = GET_UINT16();

			if (len > 0) {
				res = rho_list_make(stack - len, len);
			} else {
				res = rho_list_make(NULL, 0);
			}

			STACK_POPN(len);
			STACK_PUSH(res);
			break;
		}
		case RHO_INS_MAKE_TUPLE: {
			const unsigned int len = GET_UINT16();

			if (len > 0) {
				res = rho_tuple_make(stack - len, len);
			} else {
				res = rho_tuple_make(NULL, 0);
			}

			STACK_POPN(len);
			STACK_PUSH(res);
			break;
		}
		case RHO_INS_MAKE_SET: {
			const unsigned int len = GET_UINT16();

			if (len > 0) {
				res = rho_set_make(stack - len, len);
			} else {
				res = rho_set_make(NULL, 0);
			}

			STACK_POPN(len);

			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_PUSH(res);
			break;
		}
		case RHO_INS_MAKE_DICT: {
			const unsigned int len = GET_UINT16();

			if (len > 0) {
				res = rho_dict_make(stack - len, len);
			} else {
				res = rho_dict_make(NULL, 0);
			}

			STACK_POPN(len);

			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_PUSH(res);
			break;
		}
		case RHO_INS_IMPORT: {
			const unsigned int id = GET_UINT16();
			res = vm_import(vm, symbols.array[id].str);

			if (rho_iserror(&res)) {
				goto error;
			}

			STACK_PUSH(res);
			break;
		}
		case RHO_INS_EXPORT: {
			const unsigned int id = GET_UINT16();

			v1 = STACK_POP();

			/* no need to do a bounds check on `id`, since
			 * the preceding INS_LOAD should do all such
			 * checks
			 */
			rho_strdict_put_copy(&vm->exports,
			                 symbols.array[id].str,
			                 symbols.array[id].length,
			                 v1);
			break;
		}
		case RHO_INS_EXPORT_GLOBAL: {
			const unsigned int id = GET_UINT16();

			v1 = STACK_POP();

			/* no need to do a bounds check on `id`, since
			 * the preceding INS_LOAD_GLOBAL should do all
			 * such checks
			 */
			char *key = rho_malloc(global_symbols.array[id].length + 1);
			strcpy(key, global_symbols.array[id].str);
			rho_strdict_put(&vm->exports, key, v1, true);
			break;
		}
		case RHO_INS_GET_ITER: {
			v1 = STACK_TOP();
			res = rho_op_iter(v1);

			if (rho_iserror(&res)) {
				goto error;
			}

			rho_release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case RHO_INS_LOOP_ITER: {
			v1 = STACK_TOP();
			const unsigned int jmp = GET_UINT16();

			res = rho_op_iternext(v1);

			if (rho_iserror(&res)) {
				goto error;
			}

			if (rho_is_iter_stop(&res)) {
				pos += jmp;
			} else {
				STACK_PUSH(res);
			}

			break;
		}
		case RHO_INS_MAKE_FUNCOBJ: {
			const unsigned int num_defaults = GET_UINT16();

			RhoCodeObject *co = rho_objvalue(stack - num_defaults - 1);
			RhoValue fn = rho_funcobj_make(co);
			rho_funcobj_init_defaults(rho_objvalue(&fn), stack - num_defaults, num_defaults);

			for (unsigned i = 0; i < num_defaults; i++) {
				rho_release(STACK_POP());
			}

			STACK_SET_TOP(fn);
			rho_releaseo(co);

			break;
		}
		case RHO_INS_POP: {
			rho_release(STACK_POP());
			break;
		}
		case RHO_INS_DUP: {
			v1 = STACK_TOP();
			rho_retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case RHO_INS_DUP_TWO: {
			v1 = STACK_TOP();
			v2 = STACK_SECOND();
			rho_retain(v1);
			rho_retain(v2);
			STACK_PUSH(*v2);
			STACK_PUSH(*v1);
			break;
		}
		case RHO_INS_ROT: {
			RhoValue v1 = *STACK_SECOND();
			STACK_SET_SECOND(*STACK_TOP());
			STACK_SET_TOP(v1);
			break;
		}
		case RHO_INS_ROT_THREE: {
			RhoValue v1 = *STACK_TOP();
			RhoValue v2 = *STACK_SECOND();
			RhoValue v3 = *STACK_THIRD();
			STACK_SET_TOP(v2);
			STACK_SET_SECOND(v3);
			STACK_SET_THIRD(v1);
			break;
		}
		default: {
			RHO_INTERNAL_ERROR();
			break;
		}
		}
	}

	error:
	switch (res.type) {
	case RHO_VAL_TYPE_EXC: {
		if (EXC_STACK_EMPTY()) {
			STACK_PURGE(stack_base);
			rho_retain(&res);
			RhoException *e = rho_objvalue(&res);
			rho_exc_traceback_append(e, frame->co->name, get_lineno(frame));
			frame->return_value = res;
			return;
		} else {
			const struct rho_exc_stack_element *exc = EXC_STACK_POP();
			STACK_PURGE(exc->purge_wall);
			STACK_PUSH(res);
			pos = exc->handler_pos;
			goto head;
		}
		break;
	}
	case RHO_VAL_TYPE_ERROR: {
		Error *e = rho_errvalue(&res);
		rho_err_traceback_append(e, frame->co->name, get_lineno(frame));
		frame->return_value = res;
		return;
	}
	default:
		RHO_INTERNAL_ERROR();
	}

#undef STACK_POP
#undef STACK_TOP
#undef STACK_PUSH
}

void rho_vm_register_module(const RhoModule *module)
{
	RhoValue v = rho_makeobj((void *)module);
	rho_strdict_put(&builtin_modules_dict, module->name, &v, false);
}

static void vm_load_builtins(void)
{
	for (size_t i = 0; rho_builtins[i].name != NULL; i++) {
		rho_strdict_put(&builtins_dict, rho_builtins[i].name, (RhoValue *)&rho_builtins[i].value, false);
	}

	for (RhoClass **class = &classes[0]; *class != NULL; class++) {
		RhoValue v = rho_makeobj(*class);
		rho_strdict_put(&builtins_dict, (*class)->name, &v, false);
	}
}

static void vm_load_builtin_modules(void)
{
	for (size_t i = 0; rho_builtin_modules[i] != NULL; i++) {
		rho_vm_register_module(rho_builtin_modules[i]);
	}
}

static RhoValue vm_import(RhoVM *vm, const char *name)
{
	RhoValue cached = rho_strdict_get_cstr(&import_cache, name);

	if (!rho_isempty(&cached)) {
		rho_retain(&cached);
		return cached;
	}

	RhoCode code;
	int error = rho_load_from_file(name, false, &code);

	switch (error) {
	case RHO_LOAD_ERR_NONE:
		break;
	case RHO_LOAD_ERR_NOT_FOUND: {
		RhoValue builtin_module = rho_strdict_get_cstr(&builtin_modules_dict, name);

		if (rho_isempty(&builtin_module)) {
			return rho_import_exc_not_found(name);
		}

		return builtin_module;
	}
	case RHO_LOAD_ERR_INVALID_SIGNATURE:
		return rho_makeerr(rho_err_invalid_file_signature_error(name));
	}

	RhoVM *vm2 = rho_vm_new();
	rho_current_vm_set(vm2);
	rho_vm_exec_code(vm2, &code);
	RhoStrDict *exports = &vm2->exports;
	RhoValue mod = rho_module_make(name, exports);
	rho_strdict_put(&import_cache, name, &mod, false);
	rho_current_vm_set(vm);
	vm_link(vm, vm2);

	rho_retain(&mod);
	return mod;
}

static unsigned int get_lineno(RhoFrame *frame)
{
	const size_t raw_pos = frame->pos;
	RhoCodeObject *co = frame->co;
	unsigned int *lno_cache = co->vm->lno_cache;

	if (lno_cache != NULL && lno_cache[raw_pos] != 0) {
		return lno_cache[raw_pos];
	}

	byte *bc = co->bc;
	const byte *lno_table = co->lno_table;
	const size_t first_lineno = co->first_lineno;

	byte *p = bc;
	byte *dest = &bc[raw_pos];
	size_t ins_pos = 0;

	/* translate raw position into actual instruction position */
	while (p != dest) {
		++ins_pos;
		const int size = rho_opcode_arg_size(*p);

		if (size < 0) {
			RHO_INTERNAL_ERROR();
		}

		p += size + 1;

		if (p > dest) {
			RHO_INTERNAL_ERROR();
		}
	}

	unsigned int lineno_offset = 0;
	size_t ins_offset = 0;
	while (true) {
		const byte ins_delta = *lno_table++;
		const byte lineno_delta = *lno_table++;

		if (ins_delta == 0 && lineno_delta == 0) {
			break;
		}

		ins_offset += ins_delta;

		if (ins_offset >= ins_pos) {
			break;
		}

		lineno_offset += lineno_delta;
	}

	unsigned int lineno = first_lineno + lineno_offset;

	if (lno_cache != NULL) {
		lno_cache[raw_pos] = lineno;
	}

	return lineno;
}
