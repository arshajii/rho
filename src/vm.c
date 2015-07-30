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
#include "codeobject.h"
#include "method.h"
#include "nativefunc.h"
#include "metaclass.h"
#include "attr.h"
#include "exc.h"
#include "err.h"
#include "code.h"
#include "compiler.h"
#include "builtins.h"
#include "util.h"
#include "vmops.h"
#include "vm.h"

static Class *classes[] = {
		&obj_class,
		&int_class,
		&float_class,
		&str_class,
		&list_class,
		&tuple_class,
		&co_class,
		&method_class,
		&native_func_class,
		&meta_class,

		/* exception classes */
		&exception_class,
		&index_exception_class,
		&type_exception_class,
		&attr_exception_class,
		NULL
};

static void load_builtins(VM *vm);

VM *vm_new(void)
{
	static bool init = false;

	if (!init) {
		for (Class **class = &classes[0]; *class != NULL; class++) {
			class_init(*class);
		}
		init = true;
	}

	VM *vm = rho_malloc(sizeof(VM));
	vm->module = NULL;
	vm->callstack = NULL;
	strdict_init(&vm->builtins);
	strdict_init(&vm->exports);
	load_builtins(vm);
	return vm;
}

void vm_free(VM *vm)
{
	strdict_dealloc(&vm->exports);
	strdict_dealloc(&vm->builtins);
	free(vm);
}

/* pushes top-level frame */
static void vm_push_module_frame(VM *vm, Code *code);

static void vm_traceback(VM *vm);

void vm_pushframe(VM *vm, CodeObject *co)
{
	Frame *frame = rho_malloc(sizeof(Frame));
	frame->co = co;

	const size_t n_locals = co->names.length;
	const size_t stack_depth = co->stack_depth;
	const size_t try_catch_depth = co->try_catch_depth;

	frame->locals = rho_calloc(n_locals + stack_depth, sizeof(Value));
	frame->valuestack = frame->valuestack_base = frame->locals + n_locals;

	const size_t frees_len = co->frees.length;
	Str *frees = rho_malloc(frees_len * sizeof(Str));
	for (size_t i = 0; i < frees_len; i++) {
		frees[i] = STR_INIT(co->frees.array[i].str, co->frees.array[i].length, 0);
	}
	frame->frees = frees;

	frame->exc_stack_base = frame->exc_stack = rho_malloc(try_catch_depth * sizeof(struct exc_stack_element));

	frame->pos = 0;
	frame->prev = vm->callstack;
	frame->return_value = makeempty();
	vm->callstack = frame;
}

void vm_popframe(VM *vm)
{
	Frame *frame = vm->callstack;
	vm->callstack = frame->prev;

	const size_t n_locals = frame->co->names.length;
	Value *locals = frame->locals;

	for (size_t i = 0; i < n_locals; i++) {
		release(&locals[i]);
	}

	free(frame->locals);
	free(frame->frees);
	free(frame->exc_stack_base);
	releaseo((Object *)frame->co);
	release(&frame->return_value);
	free(frame);

	/*
	 * It's the frame evaluation function's job to make
	 * sure nothing is on the value stack before the frame
	 * is popped.
	 */
}

// TODO: move this out of vm.c
void execute(FILE *compiled)
{
	fseek(compiled, 0L, SEEK_END);
	const size_t code_size = ftell(compiled) - magic_size;
	fseek(compiled, 0L, SEEK_SET);

	/* verify magic code */
	for (size_t i = 0; i < magic_size; i++) {
		const byte c = fgetc(compiled);
		if (c != magic[i]) {
			fatal_error("verification error");
		}
	}

	VM *vm = vm_new();

	Code code;
	code_init(&code, code_size);
	fread(code.bc, 1, code_size, compiled);
	code.size = code_size;

	vm_push_module_frame(vm, &code);
	vm_eval_frame(vm);

	Value *ret = &vm->callstack->return_value;
	if (isexc(ret)) {
		Exception *e = (Exception *)objvalue(ret);
		vm_traceback(vm);
		if (e->msg != NULL) {
			fprintf(stderr, "%s: %s\n", e->base.class->name, e->msg);
		} else {
			fprintf(stderr, "%s\n", e->base.class->name);
		}
		release(ret);
	}

	vm_popframe(vm);

	vm_free(vm);
}

/*
 * Assumes the symbol table and constant table
 * have not yet been read.
 */
static void vm_push_module_frame(VM *vm, Code *code)
{
	assert(vm->module == NULL);
	CodeObject *co = codeobj_make(code, "<module>", 0, -1, -1, vm);
	vm_pushframe(vm, co);
	vm->module = vm->callstack;
}

void vm_eval_frame(VM *vm)
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
#define STACK_PURGE()        do { while (stack != stack_base) {release(STACK_POP());} } while (0)

#define EXC_STACK_PUSH(start, end, handler)  (*exc_stack++ = (struct exc_stack_element){(start), (end), (handler)})
#define EXC_STACK_POP()       (--exc_stack)
#define EXC_STACK_TOP()       (&exc_stack[-1])
#define EXC_STACK_EMPTY()     (exc_stack == exc_stack_base)

	Frame *frame = vm->callstack;
	Frame *module = vm->module;

	Value *locals = frame->locals;
	Value *globals = module->locals;
	Str *frees = frame->frees;

	const CodeObject *co = frame->co;
	struct str_array symbols = co->names;
	struct str_array attrs = co->attrs;
	struct str_array global_symbols = module->co->names;

	Value *constants = co->consts.array;
	byte *bc = co->bc;
	Value *stack_base = frame->valuestack_base;
	Value *stack = frame->valuestack;

	struct exc_stack_element *exc_stack_base = frame->exc_stack_base;
	struct exc_stack_element *exc_stack = frame->exc_stack;

	/* position in the bytecode */
	size_t pos = 0;

	Value *v1, *v2, *v3;
	Value res;

	head:
	while (true) {
		frame->pos = pos;

		while (!EXC_STACK_EMPTY() && (pos < EXC_STACK_TOP()->start || pos > EXC_STACK_TOP()->end)) {
			EXC_STACK_POP();
		}

		const byte opcode = GET_BYTE();

		switch (opcode) {
		case INS_NOP:
			break;
		case INS_LOAD_CONST: {
			const unsigned int id = GET_UINT16();
			v1 = &constants[id];
			retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case INS_LOAD_NULL: {
			STACK_PUSH(makeint(0));
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
		case INS_ADD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_add(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_SUB: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_sub(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_MUL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_mul(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_DIV: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_div(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_MOD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_mod(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_POW: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_pow(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_BITAND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_bitand(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_BITOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_bitor(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_XOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_xor(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_BITNOT: {
			v1 = STACK_TOP();
			res = op_bitnot(v1);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case INS_SHIFTL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_shiftl(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_SHIFTR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_shiftr(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_AND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_and(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_OR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_or(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_NOT: {
			v1 = STACK_TOP();
			res = op_not(v1);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_EQUAL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_eq(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_NOTEQ: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_neq(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_LT: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_lt(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_GT: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_gt(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_LE: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_le(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_GE: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ge(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_UPLUS: {
			v1 = STACK_TOP();
			res = op_plus(v1);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case INS_UMINUS: {
			v1 = STACK_TOP();
			res = op_minus(v1);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case INS_IADD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_iadd(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_ISUB: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_isub(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_IMUL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_imul(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_IDIV: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_idiv(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_IMOD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_imod(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_IPOW: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ipow(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_IBITAND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ibitand(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_IBITOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ibitor(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_IXOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ixor(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_ISHIFTL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ishiftl(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_ISHIFTR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ishiftr(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case INS_STORE: {
			v1 = STACK_POP();
			const unsigned int id = GET_UINT16();
			Value old = locals[id];
			locals[id] = *v1;
			release(&old);
			break;
		}
		case INS_LOAD: {
			const unsigned int id = GET_UINT16();
			v1 = &locals[id];

			if (isempty(v1)) {
				res = makeerr(unbound_error(symbols.array[id].str));
				goto error;
			}

			retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case INS_LOAD_GLOBAL: {
			const unsigned int id = GET_UINT16();
			v1 = &globals[id];

			if (isempty(v1)) {
				res = makeerr(unbound_error(global_symbols.array[id].str));
				goto error;
			}

			retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case INS_LOAD_ATTR: {
			v1 = STACK_TOP();
			const unsigned int id = GET_UINT16();
			const char *attr = attrs.array[id].str;
			res = op_get_attr(v1, attr);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case INS_SET_ATTR: {
			v1 = STACK_POP();
			v2 = STACK_POP();
			const unsigned int id = GET_UINT16();
			const char *attr = attrs.array[id].str;
			res = op_set_attr(v1, attr, v2);

			release(v1);
			release(v2);
			if (iserror(&res)) {
				goto error;
			}

			break;
		}
		case INS_LOAD_INDEX: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_get(v1, v2);

			release(v2);
			if (iserror(&res)) {
				goto error;
			}
			release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case INS_SET_INDEX: {
			/* X[N] = Y */
			v3 = STACK_POP();  /* N */
			v2 = STACK_POP();  /* X */
			v1 = STACK_POP();  /* Y */
			res = op_set(v2, v3, v1);

			release(&res);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			release(v3);
			break;
		}
		case INS_LOAD_NAME: {
			const unsigned int id = GET_UINT16();
			Str *key = &frees[id];
			res = strdict_get(&vm->builtins, key);

			if (!isempty(&res)) {
				retain(&res);
				STACK_PUSH(res);
				break;
			}

			res = makeerr(unbound_error(key->value));
			goto error;
			break;
		}
		case INS_PRINT: {
			v1 = STACK_POP();
			op_print(v1, stdout);
			release(v1);
			break;
		}
		case INS_JMP: {
			const unsigned int jmp = GET_UINT16();
			pos += jmp;
			break;
		}
		case INS_JMP_BACK: {
			const unsigned int jmp = GET_UINT16();
			pos -= jmp;
			break;
		}
		case INS_JMP_IF_TRUE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (resolve_nonzero(getclass(v1))(v1)) {
				pos += jmp;
			}
			release(v1);
			break;
		}
		case INS_JMP_IF_FALSE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (!resolve_nonzero(getclass(v1))(v1)) {
				pos += jmp;
			}
			release(v1);
			break;
		}
		case INS_JMP_BACK_IF_TRUE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (resolve_nonzero(getclass(v1))(v1)) {
				pos -= jmp;
			}
			release(v1);
			break;
		}
		case INS_JMP_BACK_IF_FALSE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (!resolve_nonzero(getclass(v1))(v1)) {
				pos -= jmp;
			}
			release(v1);
			break;
		}
		case INS_CALL: {
			const unsigned int argcount = GET_UINT16();
			v1 = STACK_POP();
			res = op_call(v1, stack - argcount, argcount);

			release(v1);
			if (iserror(&res)) {
				goto error;
			}

			for (unsigned int i = 0; i < argcount; i++) {
				release(STACK_POP());
			}

			STACK_PUSH(res);
			break;
		}
		case INS_RETURN: {
			v1 = STACK_POP();
			retain(v1);
			frame->return_value = *v1;
			return;
		}
		case INS_THROW: {
			v1 = STACK_POP();  // exception
			Class *class = getclass(v1);

			if (!is_subclass(class, &exception_class)) {
				res = makeerr(type_error_invalid_throw(class));
				goto error;
			}

			res = *v1;
			res.type = VAL_TYPE_EXC;
			goto error;
		}
		case INS_TRY_BEGIN: {
			const unsigned int try_block_len = GET_UINT16();
			const unsigned int handler_offset = GET_UINT16();

			EXC_STACK_PUSH(pos, pos + try_block_len, pos + handler_offset);

			break;
		}
		case INS_TRY_END: {
			EXC_STACK_POP();
			break;
		}
		case INS_JMP_IF_EXC_MISMATCH: {
			const unsigned int jmp = GET_UINT16();

			v1 = STACK_POP();  // exception type
			v2 = STACK_POP();  // exception

			Class *class = getclass(v1);

			if (class != &meta_class) {
				res = makeerr(type_error_invalid_catch(class));
				goto error;
			}

			Class *exc_type = (Class *)objvalue(v1);

			if (!is_a(v2, exc_type)) {
				pos += jmp;
			}

			release(v1);
			release(v2);

			break;
		}
		case INS_MAKE_LIST: {
			const unsigned int len = GET_UINT16();
			Value list;

			if (len > 0) {
				list = list_make(stack - len, len);
			} else {
				list = list_make(NULL, 0);
			}

			STACK_POPN(len);
			STACK_PUSH(list);
			break;
		}
		case INS_MAKE_TUPLE: {
			const unsigned int len = GET_UINT16();
			Value tup;

			if (len > 0) {
				tup = tuple_make(stack - len, len);
			} else {
				tup = tuple_make(NULL, 0);
			}

			STACK_POPN(len);
			STACK_PUSH(tup);
			break;
		}
		case INS_EXPORT: {
			const unsigned int id = GET_UINT16();

			v1 = STACK_POP();

			/* no need to do a bounds check on `id`, since
			 * the preceding INS_LOAD should do all such
			 * checks
			 */
			char *key = rho_malloc(symbols.array[id].length + 1);
			strcpy(key, symbols.array[id].str);
			strdict_put(&vm->exports, key, v1, true);
			break;
		}
		case INS_EXPORT_GLOBAL: {
			const unsigned int id = GET_UINT16();

			v1 = STACK_POP();

			/* no need to do a bounds check on `id`, since
			 * the preceding INS_LOAD_GLOBAL should do all
			 * such checks
			 */
			char *key = rho_malloc(global_symbols.array[id].length + 1);
			strcpy(key, global_symbols.array[id].str);
			strdict_put(&vm->exports, key, v1, true);
			break;
		}
		case INS_POP: {
			release(STACK_POP());
			break;
		}
		case INS_DUP: {
			v1 = STACK_TOP();
			retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case INS_DUP_TWO: {
			v1 = STACK_TOP();
			v2 = STACK_SECOND();
			retain(v1);
			retain(v2);
			STACK_PUSH(*v2);
			STACK_PUSH(*v1);
			break;
		}
		case INS_ROT: {
			Value v1 = *STACK_SECOND();
			STACK_SET_SECOND(*STACK_TOP());
			STACK_SET_TOP(v1);
			break;
		}
		case INS_ROT_THREE: {
			Value v1 = *STACK_TOP();
			Value v2 = *STACK_SECOND();
			Value v3 = *STACK_THIRD();
			STACK_SET_TOP(v2);
			STACK_SET_SECOND(v3);
			STACK_SET_THIRD(v1);
			break;
		}
		default: {
			UNEXP_BYTE(opcode);
			break;
		}
		}
	}

	error:
	switch (res.type) {
	case VAL_TYPE_EXC: {
		STACK_PURGE();

		if (EXC_STACK_EMPTY()) {
			retain(&res);
			frame->return_value = res;
			return;
		} else {
			STACK_PUSH(res);
			pos = EXC_STACK_POP()->handler_pos;
			goto head;
		}
		break;
	}
	case VAL_TYPE_ERROR: {
		vm_traceback(vm);
		Error *e = errvalue(&res);
		fflush(NULL);
		fprintf(stderr, "%s: %s\n", err_type_headers[e->type], e->msg);
		exit(EXIT_FAILURE);
		break;
	}
	default:
		INTERNAL_ERROR();
	}

#undef STACK_POP
#undef STACK_TOP
#undef STACK_PUSH
}

static void load_builtins(VM *vm)
{
	StrDict *builtins_dict = &vm->builtins;

	for (size_t i = 0; builtins[i].name != NULL; i++) {
		strdict_put(builtins_dict, builtins[i].name, (Value *)&builtins[i].value, false);
	}

	for (Class **class = &classes[0]; *class != NULL; class++) {
		Value v = makeobj(*class);
		strdict_put(builtins_dict, (*class)->name, &v, false);
	}
}

static unsigned int get_lineno(Frame *frame)
{
	const size_t raw_pos = frame->pos;
	CodeObject *co = frame->co;

	byte *bc = co->bc;
	const byte *lno_table = co->lno_table;
	const size_t first_lineno = co->first_lineno;

	byte *p = bc;
	byte *dest = &bc[raw_pos];
	size_t ins_pos = 0;

	/* translate raw position into actual instruction position */
	while (p != dest) {
		++ins_pos;
		const int size = arg_size(*p);

		if (size < 0) {
			INTERNAL_ERROR();
		}

		p += size + 1;

		if (p > dest) {
			INTERNAL_ERROR();
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

	return first_lineno + lineno_offset;
}

static void vm_traceback(VM *vm)
{
	fprintf(stderr, "Traceback:\n");
	Frame *frame = vm->callstack;

	while (frame != NULL) {
		const unsigned int lineno = get_lineno(frame);
		fprintf(stderr, "  Line %u in %s\n", lineno, frame->co->name);
		frame = frame->prev;
	}
}
