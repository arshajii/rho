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
#include "codeobject.h"
#include "method.h"
#include "attr.h"
#include "err.h"
#include "code.h"
#include "util.h"
#include "vm.h"

bool classes_init = false;

static Class *classes[] = {
		&obj_class,
		&int_class,
		&float_class,
		&str_class,
		&co_class,
		&method_class,
		NULL
};

VM *vm_new(void)
{
	if (!classes_init) {
		for (Class **class = &classes[0]; *class != NULL; class++) {
			class_init(*class);
		}
		classes_init = true;
	}

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
static void print(Value *v);

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

	const CodeObject *co = frame->co;
	struct str_array symbols = co->names;
	struct str_array attrs = co->attrs;
	struct str_array global_symbols = module->co->names;

	Value *constants = co->consts.array;
	byte *bc = co->bc;
	Value *stack = frame->valuestack;

	/* position in the bytecode */
	size_t pos = 0;

	while (true) {
		const byte opcode = bc[pos++];

		switch (opcode) {
		case INS_NOP:
			break;
		case INS_LOAD_CONST: {
			const unsigned int id = read_uint16(bc + pos);
			pos += 2;
			Value *v1 = &constants[id];
			retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case INS_LOAD_NULL: {
			STACK_PUSH(makeint(0));
			break;
		}
		case INS_ADD: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp add = resolve_add(class);

			if (!add) {
				type_error_unsupported_2("+", class, getclass(v2));
			}

			Value result = add(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_SUB: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp sub = resolve_sub(class);

			if (!sub) {
				type_error_unsupported_2("-", class, getclass(v2));
			}

			Value result = sub(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_MUL: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp mul = resolve_mul(class);

			if (!mul) {
				type_error_unsupported_2("*", class, getclass(v2));
			}

			Value result = mul(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_DIV: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp div = resolve_div(class);

			if (!div) {
				type_error_unsupported_2("/", class, getclass(v2));
			}

			Value result = div(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_MOD: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp mod = resolve_mod(class);

			if (!mod) {
				type_error_unsupported_2("%", class, getclass(v2));
			}

			Value result = mod(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_POW: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp pow = resolve_pow(class);

			if (!pow) {
				type_error_unsupported_2("**", class, getclass(v2));
			}

			Value result = pow(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_BITAND: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp and = resolve_and(class);

			if (!and) {
				type_error_unsupported_2("&", class, getclass(v2));
			}

			Value result = and(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_BITOR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp or = resolve_or(class);

			if (!or) {
				type_error_unsupported_2("|", class, getclass(v2));
			}

			Value result = or(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_XOR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp xor = resolve_xor(class);

			if (!xor) {
				type_error_unsupported_2("^", class, getclass(v2));
			}

			Value result = xor(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_BITNOT: {
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const UnOp not = resolve_not(class);

			if (!not) {
				type_error_unsupported_1("~", class);
			}

			Value result = not(v1);
			release(v1);
			STACK_PUSH(result);
			break;
		}
		case INS_SHIFTL: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp shiftl = resolve_shiftl(class);

			if (!shiftl) {
				type_error_unsupported_2("<<", class, getclass(v2));
			}

			Value result = shiftl(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_SHIFTR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BinOp shiftr = resolve_shiftr(class);

			if (!shiftr) {
				type_error_unsupported_2(">>", class, getclass(v2));
			}

			Value result = shiftr(v1, v2);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_AND: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class1 = getclass(v1);
			Class *class2 = getclass(v2);

			const BoolUnOp bool_v1 = resolve_nonzero(class1);
			const BoolUnOp bool_v2 = resolve_nonzero(class2);

			Value result = makeint(bool_v1(v1) && bool_v2(v2));
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_OR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class1 = getclass(v1);
			Class *class2 = getclass(v2);

			const BoolUnOp bool_v1 = resolve_nonzero(class1);
			const BoolUnOp bool_v2 = resolve_nonzero(class2);

			Value result = makeint(bool_v1(v1) || bool_v2(v2));
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_NOT: {
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);

			const BoolUnOp bool_v1 = resolve_nonzero(class);

			Value result = makeint(!bool_v1(v1));
			release(v1);
			STACK_PUSH(result);
			break;
		}
		case INS_EQUAL: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BoolBinOp eq = resolve_eq(class);

			if (!eq) {
				type_error_unsupported_2("==", class, getclass(v2));
			}

			Value result = makeint(eq(v1, v2));
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_NOTEQ: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const BoolBinOp eq = resolve_eq(class);

			if (!eq) {
				type_error_unsupported_2("!=", class, getclass(v2));
			}

			Value result = makeint(!eq(v1, v2));
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_LT: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const IntBinOp cmp = resolve_cmp(class);

			if (!cmp) {
				type_error_unsupported_2("<", class, getclass(v2));
			}

			Value result = makeint(cmp(v1, v2) < 0);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_GT: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const IntBinOp cmp = resolve_cmp(class);

			if (!cmp) {
				type_error_unsupported_2(">", class, getclass(v2));
			}

			Value result = makeint(cmp(v1, v2) > 0);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_LE: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const IntBinOp cmp = resolve_cmp(class);

			if (!cmp) {
				type_error_unsupported_2("<=", class, getclass(v2));
			}

			Value result = makeint(cmp(v1, v2) <= 0);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_GE: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const IntBinOp cmp = resolve_cmp(class);

			if (!cmp) {
				type_error_unsupported_2(">=", class, getclass(v2));
			}

			Value result = makeint(cmp(v1, v2) >= 0);
			release(v1);
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_UPLUS: {
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const UnOp plus = resolve_plus(class);

			if (!plus) {
				type_error_unsupported_1("+", class);
			}

			Value result = plus(v1);
			release(v1);
			STACK_PUSH(result);
			break;
		}
		case INS_UMINUS: {
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			const UnOp minus = resolve_minus(class);

			if (!minus) {
				type_error_unsupported_1("-", class);
			}

			Value result = minus(v1);
			release(v1);
			STACK_PUSH(result);
			break;
		}
		case INS_IADD: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp add = resolve_iadd(class);
			bool release_v1 = false;

			if (!add) {
				add = resolve_add(class);
				if (!add) {
					type_error_unsupported_2("+", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = add(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_ISUB: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp sub = resolve_isub(class);
			bool release_v1 = false;

			if (!sub) {
				sub = resolve_sub(class);
				if (!sub) {
					type_error_unsupported_2("-", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = sub(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_IMUL: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp mul = resolve_imul(class);
			bool release_v1 = false;

			if (!mul) {
				mul = resolve_mul(class);
				if (!mul) {
					type_error_unsupported_2("*", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = mul(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_IDIV: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp div = resolve_idiv(class);
			bool release_v1 = false;

			if (!div) {
				div = resolve_div(class);
				if (!div) {
					type_error_unsupported_2("/", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = div(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_IMOD: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp mod = resolve_imod(class);
			bool release_v1 = false;

			if (!mod) {
				mod = resolve_mod(class);
				if (!mod) {
					type_error_unsupported_2("%", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = mod(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_IPOW: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp pow = resolve_ipow(class);
			bool release_v1 = false;

			if (!pow) {
				pow = resolve_pow(class);
				if (!pow) {
					type_error_unsupported_2("**", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = pow(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_IBITAND: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp and = resolve_iand(class);
			bool release_v1 = false;

			if (!and) {
				and = resolve_and(class);
				if (!and) {
					type_error_unsupported_2("&", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = and(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_IBITOR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp or = resolve_ior(class);
			bool release_v1 = false;

			if (!or) {
				or = resolve_or(class);
				if (!or) {
					type_error_unsupported_2("|", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = or(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_IXOR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp xor = resolve_ixor(class);
			bool release_v1 = false;

			if (!xor) {
				xor = resolve_xor(class);
				if (!xor) {
					type_error_unsupported_2("^", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = xor(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_ISHIFTL: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp shiftl = resolve_ishiftl(class);
			bool release_v1 = false;

			if (!shiftl) {
				shiftl = resolve_shiftl(class);
				if (!shiftl) {
					type_error_unsupported_2("<<", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = shiftl(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_ISHIFTR: {
			Value *v2 = STACK_POP();
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			BinOp shiftr = resolve_ishiftr(class);
			bool release_v1 = false;

			if (!shiftr) {
				shiftr = resolve_shiftr(class);
				if (!shiftr) {
					type_error_unsupported_2(">>", class, getclass(v2));
				}
				release_v1 = true;
			}

			Value result = shiftr(v1, v2);
			if (release_v1) {
				release(v1);
			}
			release(v2);
			STACK_PUSH(result);
			break;
		}
		case INS_STORE: {
			Value *v1 = STACK_POP();
			const unsigned int id = read_uint16(bc + pos);
			Value old = locals[id];
			locals[id] = *v1;
			release(&old);
			pos += 2;
			break;
		}
		case INS_LOAD: {
			const unsigned int id = read_uint16(bc + pos);
			Value *v1 = &locals[id];

			if (v1->type == VAL_TYPE_EMPTY) {
				unbound_error(symbols.array[id].str);
			}

			retain(v1);
			STACK_PUSH(*v1);
			pos += 2;
			break;
		}
		case INS_LOAD_GLOBAL: {
			const unsigned int id = read_uint16(bc + pos);
			Value *v1 = &globals[id];

			if (v1->type == VAL_TYPE_EMPTY) {
				unbound_error(global_symbols.array[id].str);
			}

			retain(v1);
			STACK_PUSH(*v1);
			pos += 2;
			break;
		}
		case INS_LOAD_ATTR: {
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);

			const unsigned int id = read_uint16(bc + pos);
			pos += 2;

			const char *attr = attrs.array[id].str;

			if (!isobject(v1)) {
				goto attr_error;
			}

			const unsigned int value = attr_dict_get(&class->attr_dict, attr);

			if (!(value & ATTR_DICT_FLAG_FOUND)) {
				goto attr_error;
			}

			const bool is_method = (value & ATTR_DICT_FLAG_METHOD);
			const unsigned int idx = (value >> 2);

			Object *o = objvalue(v1);

			if (is_method) {
				const struct attr_method *method = &class->methods[idx];
				STACK_PUSH(methobj_make(o, method->meth));
			} else {
				const struct attr_member *member = &class->members[idx];
				const size_t offset = member->offset;

				switch (member->type) {
				case ATTR_T_CHAR: {
					char *c = malloc(1);
					*c = getmember(o, offset, char);
					STACK_PUSH(strobj_make((Str){.value = c, .len = 1, .hashed = 0, .freeable = 1}));
					break;
				}
				case ATTR_T_BYTE: {
					const int n = getmember(o, offset, char);
					STACK_PUSH(makeint(n));
					break;
				}
				case ATTR_T_SHORT: {
					const int n = getmember(o, offset, short);
					STACK_PUSH(makeint(n));
					break;
				}
				case ATTR_T_INT: {
					const int n = getmember(o, offset, int);
					STACK_PUSH(makeint(n));
					break;
				}
				case ATTR_T_LONG: {
					const int n = getmember(o, offset, long);
					STACK_PUSH(makeint(n));
					break;
				}
				case ATTR_T_UBYTE: {
					const int n = getmember(o, offset, unsigned char);
					STACK_PUSH(makeint(n));
					break;
				}
				case ATTR_T_USHORT: {
					const int n = getmember(o, offset, unsigned short);
					STACK_PUSH(makeint(n));
					break;
				}
				case ATTR_T_UINT: {
					const int n = getmember(o, offset, unsigned int);
					STACK_PUSH(makeint(n));
					break;
				}
				case ATTR_T_ULONG: {
					const int n = getmember(o, offset, unsigned long);
					STACK_PUSH(makeint(n));
					break;
				}
				case ATTR_T_SIZE_T: {
					const int n = getmember(o, offset, size_t);
					STACK_PUSH(makeint(n));
					break;
				}
				case ATTR_T_BOOL: {
					const int n = getmember(o, offset, bool);
					STACK_PUSH(makeint(n));
					break;
				}
				case ATTR_T_FLOAT: {
					const int n = getmember(o, offset, float);
					STACK_PUSH(makefloat(n));
					break;
				}
				case ATTR_T_DOUBLE: {
					const int n = getmember(o, offset, double);
					STACK_PUSH(makefloat(n));
					break;
				}
				case ATTR_T_STRING: {
					char *str = getmember(o, offset, char *);
					const size_t len = strlen(str);
					char *copy = malloc(len);
					memcpy(copy, str, len);
					STACK_PUSH(strobj_make((Str){.value = copy, .len = len, .hashed = 0, .freeable = 0}));
					break;
				}
				case ATTR_T_OBJECT: {
					Object *obj = getmember(o, offset, Object *);
					STACK_PUSH(makeobj(obj));
					break;
				}
				}
			}

			release(v1);
			break;

			attr_error:
			attr_error(class, attr);
			break;
		}
		case INS_PRINT: {
			Value *v1 = STACK_POP();
			print(v1);
			release(v1);
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
			if (resolve_nonzero(getclass(v1))(v1)) {
				pos += jmp;
			}
			release(v1);
			break;
		}
		case INS_JMP_IF_FALSE: {
			Value *v1 = STACK_POP();
			const unsigned int jmp = bc[pos++];
			if (!resolve_nonzero(getclass(v1))(v1)) {
				pos += jmp;
			}
			release(v1);
			break;
		}
		case INS_JMP_BACK_IF_TRUE: {
			Value *v1 = STACK_POP();
			const unsigned int jmp = bc[pos++];
			if (resolve_nonzero(getclass(v1))(v1)) {
				pos -= jmp;
			}
			release(v1);
			break;
		}
		case INS_JMP_BACK_IF_FALSE: {
			Value *v1 = STACK_POP();
			const unsigned int jmp = bc[pos++];
			if (!resolve_nonzero(getclass(v1))(v1)) {
				pos -= jmp;
			}
			release(v1);
			break;
		}
		case INS_CALL: {
			const byte argcount = bc[pos++];
			Value *v1 = STACK_POP();
			Class *class = getclass(v1);
			if (class == &co_class) {
				CodeObject *co = objvalue(v1);

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
				retain(&top->return_value);
				STACK_PUSH(top->return_value);
				vm_popframe(vm);
			} else {
				CallFunc call = resolve_call(class);

				if (!call) {
					type_error_not_callable(class);
				}

				Value result = call(v1, stack - argcount, argcount);
				retain(&result);
				STACK_PUSH(result);
				release(v1);
			}
			break;
		}
		case INS_RETURN: {
			Value *v1 = STACK_POP();
			frame->return_value = *v1;
			return;
		}
		case INS_POP: {
			release(STACK_POP());
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
		printf("%d\n", intvalue(v));
		break;
	case VAL_TYPE_FLOAT:
		printf("%f\n", floatvalue(v));
		break;
	case VAL_TYPE_OBJECT: {
		const Object *o = objvalue(v);
		const StrUnOp op = resolve_str(o->class);
		Str *str = op(v);

		printf("%s\n", str->value);

		if (str->freeable) {
			str_free(str);
		}

		break;
	}
	}
}
