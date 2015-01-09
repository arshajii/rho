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
#include "codeobject.h"
#include "method.h"
#include "attr.h"
#include "err.h"
#include "code.h"
#include "compiler.h"
#include "util.h"
#include "vmops.h"
#include "vm.h"

static Class *classes[] = {
		&obj_class,
		&int_class,
		&float_class,
		&str_class,
		&list_class,
		&co_class,
		&method_class,
		NULL
};

VM *vm_new(void)
{
	static bool init = false;

	if (!init) {
		for (Class **class = &classes[0]; *class != NULL; class++) {
			class_init(*class);
		}
		init = true;
	}

	VM *vm = malloc(sizeof(VM));
	vm->module = NULL;
	vm->callstack = NULL;
	return vm;
}

void vm_free(VM *vm)
{
	free(vm);
}

static void vm_pushframe(VM *vm, CodeObject *co);

/* pushes top-level frame */
static void vm_push_module_frame(VM *vm, Code *code);

static void vm_popframe(VM *vm);

static void vm_traceback(VM *vm);

static void vm_pushframe(VM *vm, CodeObject *co)
{
	Frame *frame = malloc(sizeof(Frame));
	frame->co = co;

	const size_t n_locals = co->names.length;
	const size_t stack_depth = co->stack_depth;
	frame->locals = calloc(n_locals + stack_depth, sizeof(Value));
	frame->valuestack = frame->locals + n_locals;
	frame->pos = 0;
	frame->prev = vm->callstack;
	vm->callstack = frame;
}

static void vm_popframe(VM *vm)
{
	Frame *frame = vm->callstack;
	vm->callstack = frame->prev;

	const size_t n_locals = frame->co->names.length;
	Value *locals = frame->locals;

	for (size_t i = 0; i < n_locals; i++) {
		release(&locals[i]);
	}

	free(frame->locals);
	releaseo((Object *)frame->co);
	release(&frame->return_value);
	free(frame);
}

static void eval_frame(VM *vm);

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
	eval_frame(vm);
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
	CodeObject *co = codeobj_make(code, "<module>", 0, -1);
	vm_pushframe(vm, co);
	vm->module = vm->callstack;
}

static unsigned int read_uint16_from_bc(byte *bc, size_t *pos)
{
	const unsigned int n = read_uint16_from_stream(bc + *pos);
	(*pos) += 2;
	return n;
}

static void eval_frame(VM *vm)
{
#define GET_BYTE() (bc[pos++])
#define GET_UINT16() (read_uint16_from_bc(bc, &pos))

#define STACK_POP()          (--stack)
#define STACK_POPN(n)        (stack -= (n))
#define STACK_TOP()          (&stack[-1])
#define STACK_SECOND()       (&stack[-2])
#define STACK_THIRD()        (&stack[-3])
#define STACK_PUSH(v)        (*stack++ = (v))
#define STACK_SET_TOP(v)     (stack[-1] = (v))
#define STACK_SET_SECOND(v)  (stack[-2] = (v))
#define STACK_SET_THIRD(v)   (stack[-3] = (v))

	Frame *frame = vm->callstack;
	Frame *module = vm->module;

	Value *locals = frame->locals;
	Value *globals = module->locals;

	const CodeObject *co = frame->co;
	struct str_array symbols = co->names;
	struct str_array attrs = co->attrs;
	struct str_array global_symbols = module->co->names;

	Value *constants = co->consts.array;
	byte *bc = co->bc;
	Value *stack = frame->valuestack;

	/* position in the bytecode */
	size_t pos = 0;

	Value *v1, *v2, *v3;
	Value res;

	while (true) {
		frame->pos = pos;
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
		case INS_ADD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_add(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_SUB: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_sub(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_MUL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_mul(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_DIV: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_div(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_MOD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_mod(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_POW: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_pow(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_BITAND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_bitand(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_BITOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_bitor(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_XOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_xor(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
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

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_SHIFTR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_shiftr(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_AND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_and(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_OR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_or(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_NOT: {
			v1 = STACK_TOP();
			res = op_not(v1);

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

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_NOTEQ: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_neq(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_LT: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_lt(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_GT: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_gt(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_LE: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_le(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_GE: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ge(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
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

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_ISUB: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_isub(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_IMUL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_imul(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_IDIV: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_idiv(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_IMOD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_imod(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_IPOW: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ipow(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_IBITAND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ibitand(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_IBITOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ibitor(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_IXOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ixor(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_ISHIFTL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ishiftl(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_ISHIFTR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_ishiftr(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v2);
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

			if (v1->type == VAL_TYPE_EMPTY) {
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

			if (v1->type == VAL_TYPE_EMPTY) {
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

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			break;
		}
		case INS_LOAD_INDEX: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = op_get(v1, v2);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			release(v2);
			STACK_SET_TOP(res);
			break;
		}
		case INS_SET_INDEX: {
			/* X[N] = Y */
			v3 = STACK_POP();  /* N */
			v2 = STACK_POP();  /* X */
			v1 = STACK_POP();  /* Y */
			res = op_set(v2, v3, v1);

			if (iserror(&res)) {
				goto error;
			}

			release(&res);
			release(v1);
			release(v2);
			release(v3);
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
			Class *class = getclass(v1);

			if (class == &co_class) {
				CodeObject *co = objvalue(v1);

				if (co->argcount != argcount) {
					res = makeerr(call_error_args(co->name, co->argcount, argcount));
					goto error;
				}

				vm_pushframe(vm, co);

				Frame *top = vm->callstack;
				for (unsigned int i = 0; i < argcount; i++) {
					top->locals[argcount - i - 1] = *STACK_POP();
				}

				eval_frame(vm);
				res = top->return_value;

				if (iserror(&res)) {
					goto error;
				}

				STACK_PUSH(res);
				vm_popframe(vm);
			} else {
				CallFunc call = resolve_call(class);

				if (!call) {
					res = makeerr(type_error_not_callable(class));
					goto error;
				}

				res = call(v1, stack - argcount, argcount);

				if (iserror(&res)) {
					goto error;
				}

				for (unsigned int i = 0; i < argcount; i++) {
					release(STACK_POP());
				}

				release(v1);
				STACK_PUSH(res);
			}
			break;
		}
		case INS_RETURN: {
			v1 = STACK_POP();
			retain(v1);
			frame->return_value = *v1;
			return;
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
	assert(iserror(&res));
	vm_traceback(vm);
	Error *e = errvalue(&res);
	fflush(NULL);
	fprintf(stderr, "%s: ", err_type_headers[e->type]);
	fprintf(stderr, "%s\n", e->msg);
	exit(EXIT_FAILURE);

#undef STACK_POP
#undef STACK_TOP
#undef STACK_PUSH
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
		int size = arg_size(*p);

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
