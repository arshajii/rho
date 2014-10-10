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
#include "compiler.h"
#include "util.h"
#include "vmops.h"
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
#define STACK_TOP()          (&stack[-1])
#define STACK_SECOND()       (&stack[-2])
#define STACK_PUSH(v)        (*stack++ = (v))
#define STACK_SET_TOP(v)     (stack[-1] = (v))
#define STACK_SET_SECOND(v)  (stack[-2] = (v))

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

	Value *v1, *v2;
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
			res = op_uplus(v1);

			if (iserror(&res)) {
				goto error;
			}

			release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case INS_UMINUS: {
			v1 = STACK_TOP();
			res = op_uminus(v1);

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
			Class *class = getclass(v1);

			const unsigned int id = GET_UINT16();

			const char *attr = attrs.array[id].str;

			if (!isobject(v1)) {
				goto get_attr_error_not_found;
			}

			const unsigned int value = attr_dict_get(&class->attr_dict, attr);

			if (!(value & ATTR_DICT_FLAG_FOUND)) {
				goto get_attr_error_not_found;
			}

			const bool is_method = (value & ATTR_DICT_FLAG_METHOD);
			const unsigned int idx = (value >> 2);

			Object *o = objvalue(v1);

			if (is_method) {
				const struct attr_method *method = &class->methods[idx];
				STACK_SET_TOP(methobj_make(o, method->meth));
			} else {
				const struct attr_member *member = &class->members[idx];
				const size_t offset = member->offset;

				switch (member->type) {
				case ATTR_T_CHAR: {
					char *c = malloc(1);
					*c = getmember(o, offset, char);
					STACK_SET_TOP(strobj_make(STR_INIT(c, 1, 1)));
					break;
				}
				case ATTR_T_BYTE: {
					const long n = getmember(o, offset, char);
					STACK_SET_TOP(makeint(n));
					break;
				}
				case ATTR_T_SHORT: {
					const long n = getmember(o, offset, short);
					STACK_SET_TOP(makeint(n));
					break;
				}
				case ATTR_T_INT: {
					const long n = getmember(o, offset, int);
					STACK_SET_TOP(makeint(n));
					break;
				}
				case ATTR_T_LONG: {
					const long n = getmember(o, offset, long);
					STACK_SET_TOP(makeint(n));
					break;
				}
				case ATTR_T_UBYTE: {
					const long n = getmember(o, offset, unsigned char);
					STACK_SET_TOP(makeint(n));
					break;
				}
				case ATTR_T_USHORT: {
					const long n = getmember(o, offset, unsigned short);
					STACK_SET_TOP(makeint(n));
					break;
				}
				case ATTR_T_UINT: {
					const long n = getmember(o, offset, unsigned int);
					STACK_SET_TOP(makeint(n));
					break;
				}
				case ATTR_T_ULONG: {
					const long n = getmember(o, offset, unsigned long);
					STACK_SET_TOP(makeint(n));
					break;
				}
				case ATTR_T_SIZE_T: {
					const long n = getmember(o, offset, size_t);
					STACK_SET_TOP(makeint(n));
					break;
				}
				case ATTR_T_BOOL: {
					const long n = getmember(o, offset, bool);
					STACK_SET_TOP(makeint(n));
					break;
				}
				case ATTR_T_FLOAT: {
					const double d = getmember(o, offset, float);
					STACK_SET_TOP(makefloat(d));
					break;
				}
				case ATTR_T_DOUBLE: {
					const double d = getmember(o, offset, double);
					STACK_SET_TOP(makefloat(d));
					break;
				}
				case ATTR_T_STRING: {
					char *str = getmember(o, offset, char *);
					const size_t len = strlen(str);
					char *copy = malloc(len);
					memcpy(copy, str, len);
					STACK_SET_TOP(strobj_make(STR_INIT(copy, len, 1)));
					break;
				}
				case ATTR_T_OBJECT: {
					Object *obj = getmember(o, offset, Object *);
					STACK_SET_TOP(makeobj(obj));
					break;
				}
				}
			}

			release(v1);
			break;

			get_attr_error_not_found:
			res = makeerr(attr_error_not_found(class, attr));
			goto error;

			break;
		}
		case INS_SET_ATTR: {
			v1 = STACK_POP();
			v2 = STACK_POP();
			Class *v1_class = getclass(v1);
			Class *v2_class = getclass(v2);

			const unsigned int id = GET_UINT16();

			const char *attr = attrs.array[id].str;

			if (!isobject(v1)) {
				goto set_attr_error_not_found;
			}

			const unsigned int value = attr_dict_get(&v1_class->attr_dict, attr);

			if (!(value & ATTR_DICT_FLAG_FOUND)) {
				goto set_attr_error_not_found;
			}

			const bool is_method = (value & ATTR_DICT_FLAG_METHOD);

			if (is_method) {
				goto set_attr_error_readonly;
			}

			const unsigned int idx = (value >> 2);

			const struct attr_member *member = &v1_class->members[idx];

			const int member_flags = member->flags;

			if (member_flags & ATTR_FLAG_READONLY) {
				goto set_attr_error_readonly;
			}

			const size_t offset = member->offset;

			Object *o = objvalue(v1);
			char *o_raw = (char *)o;

			switch (member->type) {
			case ATTR_T_CHAR: {
				if (v2_class != &str_class) {
					goto set_attr_error_mismatch;
				}

				StrObject *str = objvalue(v2);
				const size_t len = str->str.len;

				if (len != 1) {
					goto set_attr_error_mismatch;
				}

				const char c = str->str.value[0];
				char *member_raw = (char *)(o_raw + offset);
				*member_raw = c;
				break;
			}
			case ATTR_T_BYTE: {
				if (!isint(v2)) {
					goto set_attr_error_mismatch;
				}
				const long n = intvalue(v2);
				char *member_raw = (char *)(o_raw + offset);
				*member_raw = (char)n;
				break;
			}
			case ATTR_T_SHORT: {
				if (!isint(v2)) {
					goto set_attr_error_mismatch;
				}
				const long n = intvalue(v2);
				short *member_raw = (short *)(o_raw + offset);
				*member_raw = (short)n;
				break;
			}
			case ATTR_T_INT: {
				if (!isint(v2)) {
					goto set_attr_error_mismatch;
				}
				const long n = intvalue(v2);
				int *member_raw = (int *)(o_raw + offset);
				*member_raw = (int)n;
				break;
			}
			case ATTR_T_LONG: {
				if (!isint(v2)) {
					goto set_attr_error_mismatch;
				}
				const long n = intvalue(v2);
				long *member_raw = (long *)(o_raw + offset);
				*member_raw = n;
				break;
			}
			case ATTR_T_UBYTE: {
				if (!isint(v2)) {
					goto set_attr_error_mismatch;
				}
				const long n = intvalue(v2);
				unsigned char *member_raw = (unsigned char *)(o_raw + offset);
				*member_raw = (unsigned char)n;
				break;
			}
			case ATTR_T_USHORT: {
				if (!isint(v2)) {
					goto set_attr_error_mismatch;
				}
				const long n = intvalue(v2);
				unsigned short *member_raw = (unsigned short *)(o_raw + offset);
				*member_raw = (unsigned short)n;
				break;
			}
			case ATTR_T_UINT: {
				if (!isint(v2)) {
					goto set_attr_error_mismatch;
				}
				const long n = intvalue(v2);
				unsigned int *member_raw = (unsigned int *)(o_raw + offset);
				*member_raw = (unsigned int)n;
				break;
			}
			case ATTR_T_ULONG: {
				if (!isint(v2)) {
					goto set_attr_error_mismatch;
				}
				const long n = intvalue(v2);
				unsigned long *member_raw = (unsigned long *)(o_raw + offset);
				*member_raw = (unsigned long)n;
				break;
			}
			case ATTR_T_SIZE_T: {
				if (!isint(v2)) {
					goto set_attr_error_mismatch;
				}
				const long n = intvalue(v2);
				size_t *member_raw = (size_t *)(o_raw + offset);
				*member_raw = (size_t)n;
				break;
			}
			case ATTR_T_BOOL: {
				if (!isint(v2)) {
					goto set_attr_error_mismatch;
				}
				const long n = intvalue(v2);
				bool *member_raw = (bool *)(o_raw + offset);
				*member_raw = (bool)n;
				break;
			}
			case ATTR_T_FLOAT: {
				float d;

				/* ints get promoted */
				if (isint(v2)) {
					d = (float)intvalue(v2);
				} else if (!isfloat(v2)) {
					d = 0;
					goto set_attr_error_mismatch;
				} else {
					d = (float)floatvalue(v2);
				}

				float *member_raw = (float *)(o_raw + offset);
				*member_raw = d;
				break;
			}
			case ATTR_T_DOUBLE: {
				double d;

				/* ints get promoted */
				if (isint(v2)) {
					d = (double)intvalue(v2);
				} else if (!isfloat(v2)) {
					d = 0;
					goto set_attr_error_mismatch;
				} else {
					d = floatvalue(v2);
				}

				float *member_raw = (float *)(o_raw + offset);
				*member_raw = d;
				break;
			}
			case ATTR_T_STRING: {
				if (v2_class != &str_class) {
					goto set_attr_error_mismatch;
				}

				StrObject *str = objvalue(v2);

				char **member_raw = (char **)(o_raw + offset);
				*member_raw = (char *)str->str.value;
				break;
			}
			case ATTR_T_OBJECT: {
				if (!isobject(v2)) {
					goto set_attr_error_mismatch;
				}

				Object **member_raw = (Object **)(o_raw + offset);

				if ((member_flags & ATTR_FLAG_TYPE_STRICT) &&
				    ((*member_raw)->class != v2_class)) {

					goto set_attr_error_mismatch;
				}

				Object *o2 = objvalue(v2);
				retaino(o2);
				*member_raw = o2;
				break;
			}
			}

			break;

			set_attr_error_not_found:
			res = makeerr(attr_error_not_found(v1_class, attr));
			goto error;

			set_attr_error_readonly:
			res = makeerr(attr_error_readonly(v1_class, attr));
			goto error;

			set_attr_error_mismatch:
			res = makeerr(attr_error_mismatch(v1_class, attr, v2_class));
			goto error;

			break;
		}
		case INS_PRINT: {
			v1 = STACK_POP();
			print(v1);
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
					top->locals[i] = *STACK_POP();
				}

				eval_frame(vm);
				retain(&top->return_value);
				STACK_PUSH(top->return_value);
				vm_popframe(vm);
			} else {
				CallFunc call = resolve_call(class);

				if (!call) {
					res = makeerr(type_error_not_callable(class));
					goto error;
				}

				Value result = call(v1, stack - argcount, argcount);
				retain(&result);
				for (unsigned int i = 0; i < argcount; i++) {
					release(STACK_POP());
				}
				STACK_PUSH(result);
				release(v1);
			}
			break;
		}
		case INS_RETURN: {
			v1 = STACK_POP();
			frame->return_value = *v1;
			return;
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
		case INS_ROT: {
			Value v1 = *STACK_SECOND();
			STACK_SET_SECOND(*STACK_TOP());
			STACK_SET_TOP(v1);
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

static void print(Value *v)
{
	switch (v->type) {
	case VAL_TYPE_INT:
		printf("%ld\n", intvalue(v));
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
	case VAL_TYPE_EMPTY:
	case VAL_TYPE_ERROR:
	case VAL_TYPE_UNSUPPORTED_TYPES:
	case VAL_TYPE_DIV_BY_ZERO:
		INTERNAL_ERROR();
		break;
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
