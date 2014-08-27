#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "compiler.h"
#include "opcodes.h"
#include "str.h"
#include "strobject.h"
#include "err.h"
#include "code.h"
#include "util.h"
#include "vm.h"

VM *vm_new(void)
{
	VM *vm = malloc(sizeof(VM));
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

static void vm_pushframe(VM *vm,
                         CodeObject *co)
{
	/* XXX: maintain a bytecode address to Frame
	 * dictionary to avoid creating frames anew on
	 * each function call.
	 */

	Frame *frame = malloc(sizeof(Frame));
	frame->co = co;

	frame->locals = calloc(co->names.length, sizeof(Value));
	frame->valuestack = malloc(DEFAULT_VSTACK_DEPTH * sizeof(Value));
	frame->stack_capacity = DEFAULT_VSTACK_DEPTH;

	frame->prev = vm->callstack;
	vm->callstack = frame;
}

static void vm_popframe(VM *vm)
{
	Frame *frame = vm->callstack;
	vm->callstack = frame->prev;
	//free(frame->local_symtab.array);

	//for (Value *v = frame->constants; v < frame->locals; ++v) {
		//destroy(v);
	//}

	/*
	for (Value *v = frame->locals; v < frame->valuestack; ++v) {
		decref(v);
	}
	*/

	free(frame);
}

static void eval_frame(VM *vm);
static void print(Value *v);

// TODO: move this out of vm.c
void execute(FILE *compiled)
{
	fseek(compiled, 0L, SEEK_END);
	const size_t code_size = ftell(compiled);
	fseek(compiled, 0L, SEEK_SET);

	VM *vm = vm_new();

	Code code_0;
	code_init(&code_0, code_size);
	fread(code_0.bc, sizeof(char), code_size, compiled);
	code_0.size += code_size;

	Code code = code_0;

	/* verify magic code */
	for (size_t i = 0; i < magic_size; i++) {
		if (code_read_byte(&code) != magic[i]) {
			fatal_error("verification error");
		}
	}

	vm_push_module_frame(vm, &code);
	eval_frame(vm);
	vm_popframe(vm);

	vm_free(vm);
	code_dealloc(&code_0);
}

/*
 * Assumes the symbol table and constant table
 * have not yet been read.
 */
static void vm_push_module_frame(VM *vm, Code *code)
{
	CodeObject *co = codeobj_make(code, "<module>", 0);

	vm_pushframe(
		vm,
		co
	);

	vm->module = vm->callstack;
}

/* utility function for pushing onto value stack */
static void frame_vstack_push(Frame *frame, Value v, Value **stack)
{
	Value *stack_ptr = *stack;
	if ((size_t)(stack_ptr - frame->valuestack) == frame->stack_capacity) {
		assert(0);
		// TODO: this results in invalid pointers
		//const size_t new_capacity = (((stack_ptr - frame->constants) * 3)/2 + 1) * sizeof(Value);
		//frame->constants = realloc(frame->constants, new_capacity);
		//frame->stack_capacity = new_capacity;
	}

	**stack = v;
	++*stack;
}

static void eval_frame(VM *vm)
{
#define STACK_POP() (--stack)
#define STACK_PEEK() (&stack[-1])
#define STACK_PUSH(v) (frame_vstack_push(frame, (v), &stack))

	Frame *frame = vm->callstack;
	Frame *module = vm->module;

	Value *locals = frame->locals;
	Value *globals = module->locals;

	struct str_array symbols = frame->co->names;
	struct str_array global_symbols = module->co->names;

	Value *constants = frame->co->consts.array;
	byte *bc = frame->co->bc;
	Value *stack = frame->valuestack;

	/* position in the bytecode */
	size_t pos = 0;

	while (true) {
		const byte opcode = bc[pos++];

		switch (opcode) {
		case INS_NOP:
			break;
		case INS_LOAD_CONST: {
			const unsigned int val = bc[pos++];
			STACK_PUSH(constants[val]);
			break;
		}
		case INS_LOAD_NULL: {
			STACK_PUSH(makeint(0));
			break;
		}
		case INS_ADD: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp add = resolve_add(class);

			if (!add) {
				type_error_unsupported_2("+", class, getclass(v2));
			}

			STACK_PUSH(add(v1, v2));
			break;
		}
		case INS_SUB: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp sub = resolve_sub(class);

			if (!sub) {
				type_error_unsupported_2("-", class, getclass(v2));
			}

			STACK_PUSH(sub(v1, v2));
			break;
		}
		case INS_MUL: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp mul = resolve_mul(class);

			if (!mul) {
				type_error_unsupported_2("*", class, getclass(v2));
			}

			STACK_PUSH(mul(v1, v2));
			break;
		}
		case INS_DIV: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp div = resolve_div(class);

			if (!div) {
				type_error_unsupported_2("/", class, getclass(v2));
			}

			STACK_PUSH(div(v1, v2));
			break;
		}
		case INS_MOD: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp mod = resolve_mod(class);

			if (!mod) {
				type_error_unsupported_2("%", class, getclass(v2));
			}

			STACK_PUSH(mod(v1, v2));
			break;
		}
		case INS_POW: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp pow = resolve_pow(class);

			if (!pow) {
				type_error_unsupported_2("**", class, getclass(v2));
			}

			STACK_PUSH(pow(v1, v2));
			break;
		}
		case INS_BITAND: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp and = resolve_and(class);

			if (!and) {
				type_error_unsupported_2("&", class, getclass(v2));
			}

			STACK_PUSH(and(v1, v2));
			break;
		}
		case INS_BITOR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp or = resolve_or(class);

			if (!or) {
				type_error_unsupported_2("|", class, getclass(v2));
			}

			STACK_PUSH(or(v1, v2));
			break;
		}
		case INS_XOR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp xor = resolve_xor(class);

			if (!xor) {
				type_error_unsupported_2("^", class, getclass(v2));
			}

			STACK_PUSH(xor(v1, v2));
			break;
		}
		case INS_BITNOT: {
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const UnOp not = resolve_not(class);

			if (!not) {
				type_error_unsupported_1("~", class);
			}

			STACK_PUSH(not(v1));
			break;
		}
		case INS_SHIFTL: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp shiftl = resolve_shiftl(class);

			if (!shiftl) {
				type_error_unsupported_2("<<", class, getclass(v2));
			}

			STACK_PUSH(shiftl(v1, v2));
			break;
		}
		case INS_SHIFTR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BinOp shiftr = resolve_shiftr(class);

			if (!shiftr) {
				type_error_unsupported_2(">>", class, getclass(v2));
			}

			STACK_PUSH(shiftr(v1, v2));
			break;
		}
		case INS_AND: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class1 = getclass(v1);
			const Class *class2 = getclass(v2);

			const BoolUnOp bool_v1 = resolve_nonzero(class1);

			if (!bool_v1) {
				type_error_unsupported_1("bool", class1);
			}

			const BoolUnOp bool_v2 = resolve_nonzero(class2);

			if (!bool_v2) {
				type_error_unsupported_1("bool", class2);
			}

			STACK_PUSH(makeint(bool_v1(v1) && bool_v2(v2)));
			break;
		}
		case INS_OR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class1 = getclass(v1);
			const Class *class2 = getclass(v2);

			const BoolUnOp bool_v1 = resolve_nonzero(class1);

			if (!bool_v1) {
				type_error_unsupported_1("bool", class1);
			}

			const BoolUnOp bool_v2 = resolve_nonzero(class2);

			if (!bool_v2) {
				type_error_unsupported_1("bool", class2);
			}

			STACK_PUSH(makeint(bool_v1(v1) || bool_v2(v2)));
			break;
		}
		case INS_NOT: {
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);

			const BoolUnOp bool_v1 = resolve_nonzero(class);

			if (!bool_v1) {
				type_error_unsupported_1("bool", class);
			}

			STACK_PUSH(makeint(!bool_v1(v1)));
			break;
		}
		case INS_EQUAL: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BoolBinOp eq = resolve_eq(class);

			if (!eq) {
				type_error_unsupported_2("==", class, getclass(v2));
			}

			STACK_PUSH(makeint(eq(v1, v2)));
			break;
		}
		case INS_NOTEQ: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const BoolBinOp eq = resolve_eq(class);

			if (!eq) {
				type_error_unsupported_2("!=", class, getclass(v2));
			}

			STACK_PUSH(makeint(!eq(v1, v2)));
			break;
		}
		case INS_LT: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const IntBinOp cmp = resolve_cmp(class);

			if (!cmp) {
				type_error_unsupported_2("<", class, getclass(v2));
			}

			STACK_PUSH(makeint(cmp(v1, v2) < 0));
			break;
		}
		case INS_GT: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const IntBinOp cmp = resolve_cmp(class);

			if (!cmp) {
				type_error_unsupported_2(">", class, getclass(v2));
			}

			STACK_PUSH(makeint(cmp(v1, v2) > 0));
			break;
		}
		case INS_LE: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const IntBinOp cmp = resolve_cmp(class);

			if (!cmp) {
				type_error_unsupported_2("<=", class, getclass(v2));
			}

			STACK_PUSH(makeint(cmp(v1, v2) <= 0));
			break;
		}
		case INS_GE: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const IntBinOp cmp = resolve_cmp(class);

			if (!cmp) {
				type_error_unsupported_2(">=", class, getclass(v2));
			}

			STACK_PUSH(makeint(cmp(v1, v2) >= 0));
			break;
		}
		case INS_UPLUS: {
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const UnOp plus = resolve_plus(class);

			if (!plus) {
				type_error_unsupported_1("+", class);
			}

			STACK_PUSH(plus(v1));
			break;
		}
		case INS_UMINUS: {
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			const UnOp minus = resolve_minus(class);

			if (!minus) {
				type_error_unsupported_1("-", class);
			}

			STACK_PUSH(minus(v1));
			break;
		}
		case INS_IADD: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp add = resolve_iadd(class);

			if (!add) {
				add = resolve_add(class);
				if (!add) {
					type_error_unsupported_2("+", class, getclass(v2));
				}
			}

			STACK_PUSH(add(v1, v2));
			break;
		}
		case INS_ISUB: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp sub = resolve_isub(class);

			if (!sub) {
				sub = resolve_sub(class);
				if (!sub) {
					type_error_unsupported_2("-", class, getclass(v2));
				}
			}

			STACK_PUSH(sub(v1, v2));
			break;
		}
		case INS_IMUL: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp mul = resolve_imul(class);

			if (!mul) {
				mul = resolve_mul(class);
				if (!mul) {
					type_error_unsupported_2("*", class, getclass(v2));
				}
			}

			STACK_PUSH(mul(v1, v2));
			break;
		}
		case INS_IDIV: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp div = resolve_idiv(class);

			if (!div) {
				div = resolve_div(class);
				if (!div) {
					type_error_unsupported_2("/", class, getclass(v2));
				}
			}

			STACK_PUSH(div(v1, v2));
			break;
		}
		case INS_IMOD: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp mod = resolve_imod(class);

			if (!mod) {
				mod = resolve_mod(class);
				if (!mod) {
					type_error_unsupported_2("%", class, getclass(v2));
				}
			}

			STACK_PUSH(mod(v1, v2));
			break;
		}
		case INS_IPOW: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp pow = resolve_ipow(class);

			if (!pow) {
				pow = resolve_pow(class);
				if (!pow) {
					type_error_unsupported_2("**", class, getclass(v2));
				}
			}

			STACK_PUSH(pow(v1, v2));
			break;
		}
		case INS_IBITAND: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp and = resolve_iand(class);

			if (!and) {
				and = resolve_and(class);
				if (!and) {
					type_error_unsupported_2("&", class, getclass(v2));
				}
			}

			STACK_PUSH(and(v1, v2));
			break;
		}
		case INS_IBITOR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp or = resolve_ior(class);

			if (!or) {
				or = resolve_or(class);
				if (!or) {
					type_error_unsupported_2("|", class, getclass(v2));
				}
			}

			STACK_PUSH(or(v1, v2));
			break;
		}
		case INS_IXOR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp xor = resolve_ixor(class);

			if (!xor) {
				xor = resolve_xor(class);
				if (!xor) {
					type_error_unsupported_2("^", class, getclass(v2));
				}
			}

			STACK_PUSH(xor(v1, v2));
			break;
		}
		case INS_ISHIFTL: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp shiftl = resolve_ishiftl(class);

			if (!shiftl) {
				shiftl = resolve_shiftl(class);
				if (!shiftl) {
					type_error_unsupported_2("<<", class, getclass(v2));
				}
			}

			STACK_PUSH(shiftl(v1, v2));
			break;
		}
		case INS_ISHIFTR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			const Class *class = getclass(v1);
			BinOp shiftr = resolve_ishiftr(class);

			if (!shiftr) {
				shiftr = resolve_shiftr(class);
				if (!shiftr) {
					type_error_unsupported_2(">>", class, getclass(v2));
				}
			}

			STACK_PUSH(shiftr(v1, v2));
			break;
		}
		case INS_STORE: {
			Value *v1 = STACK_POP();
			const unsigned int id = read_int(bc + pos);
			locals[id] = *v1;
			incref(v1);
			pos += INT_SIZE;
			break;
		}
		case INS_LOAD: {
			const unsigned int id = read_int(bc + pos);

			if (locals[id].type == VAL_TYPE_EMPTY) {
				unbound_error(symbols.array[id].str);
			}

			STACK_PUSH(locals[id]);
			pos += INT_SIZE;
			break;
		}
		case INS_LOAD_GLOBAL: {
			const unsigned int id = read_int(bc + pos);

			if (globals[id].type == VAL_TYPE_EMPTY) {
				unbound_error(global_symbols.array[id].str);
			}

			STACK_PUSH(globals[id]);
			pos += INT_SIZE;
			break;
		}
		case INS_PRINT: {
			print(STACK_POP());
			break;
		}
		case INS_JMP: {
			const unsigned int jmp = bc[pos++];
			pos += jmp;
			break;
		}
		case INS_JMP_BACK: {
			const unsigned int jmp = bc[pos++];
			pos -= jmp;
			break;
		}
		case INS_JMP_IF_TRUE: {
			Value *v1 = STACK_POP();
			const unsigned int jmp = bc[pos++];
			if (v1->data.i != 0) {
				pos += jmp;
			}
			break;
		}
		case INS_JMP_IF_FALSE: {
			Value *v1 = STACK_POP();
			const unsigned int jmp = bc[pos++];
			if (v1->data.i == 0) {
				pos += jmp;
			}
			break;
		}
		case INS_JMP_BACK_IF_TRUE: {
			Value *v1 = STACK_POP();
			const unsigned int jmp = bc[pos++];
			if (v1->data.i != 0) {
				pos -= jmp;
			}
			break;
		}
		case INS_JMP_BACK_IF_FALSE: {
			Value *v1 = STACK_POP();
			const unsigned int jmp = bc[pos++];
			if (v1->data.i == 0) {
				pos -= jmp;
			}
			break;
		}
		case INS_CALL: {
			const byte argcount = bc[pos++];
			Value *v1 = STACK_POP();
			CodeObject *co = v1->data.o;

			if (co->argcount != argcount) {
				call_error_args(co->name, co->argcount, argcount);
			}

			vm_pushframe(vm, co);

			Frame *top = vm->callstack;
			for (byte i = 0; i < argcount; i++) {
				Value *v = STACK_POP();
				top->locals[i] = *v;
			}

			eval_frame(vm);
			STACK_PUSH(top->return_value);
			vm_popframe(vm);
			break;
		}
		case INS_RETURN: {
			frame->return_value = *STACK_POP();
			return;
		}
		case INS_POP: {
			STACK_POP();
			break;
		}
		default: {
			UNEXP_BYTE(opcode);
			break;
		}
		}
	}

#undef STACK_POP
#undef STACK_PEEK
#undef STACK_PUSH
}

static void print(Value *v)
{
	switch (v->type) {
	case VAL_TYPE_EMPTY:
		fatal_error("unexpected value type VAL_TYPE_EMPTY");
		break;
	case VAL_TYPE_INT:
		printf("%d\n", v->data.i);
		break;
	case VAL_TYPE_FLOAT:
		printf("%f\n", v->data.f);
		break;
	case VAL_TYPE_OBJECT: {
		Object *o = v->data.o;
		printf("%s\n", ((StrObject *) o->class->str(v).data.o)->str.value);
		break;
	}
	}
}
