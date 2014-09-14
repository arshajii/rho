#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "symtab.h"
#include "opcodes.h"
#include "code.h"
#include "compiler.h"

struct metadata {
	unsigned int max_vstack_depth;
};

static void write_byte(Compiler *compiler, byte p)
{
	code_write_byte(&compiler->code, p);
}

void write_int(Compiler *c, const int n)
{
	code_write_int(&c->code, n);
}

void write_uint16(Compiler *c, const size_t n)
{
	code_write_uint16(&c->code, n);
}

void write_uint16_at(Compiler *c, const size_t n, const size_t pos)
{
	code_write_uint16_at(&c->code, n, pos);
}

void write_double(Compiler *c, const double d)
{
	code_write_double(&c->code, d);
}

const byte magic[] = {0xFE, 0xED, 0xF0, 0x0D};
const size_t magic_size = sizeof(magic);

/*
 * Compilation
 */
static void fill_ct(Compiler *compiler, Program *program);
static void write_sym_table(Compiler *compiler);
static void write_const_table(Compiler *compiler);

static Opcode to_opcode(NodeType type);

static Compiler *compiler_new(const char *filename, SymTable *st)
{
	Compiler *compiler = malloc(sizeof(Compiler));
	compiler->filename = filename;
	code_init(&compiler->code, DEFAULT_BC_CAPACITY);
	compiler->st = st;
	compiler->ct = ct_new();

	return compiler;
}

/*
 * Note: this does not deallocate the compiler's code
 * field.
 */
static void compiler_free(Compiler *compiler, bool free_st)
{
	if (free_st) {
		st_free(compiler->st);
	}
	ct_free(compiler->ct);
	code_dealloc(&compiler->code);
	free(compiler);
}

static void compile_node(Compiler *compiler, AST *ast, bool toplevel);
static struct metadata compile_program(Compiler *compiler, Program *program);

static void compile_load(Compiler *compiler, AST *ast);
static void compile_assignment(Compiler *compiler, AST *ast);

static void compile_call(Compiler *compiler, AST *ast);

static void compile_block(Compiler *compiler, AST *ast);
static void compile_if(Compiler *compiler, AST *ast);
static void compile_while(Compiler *compiler, AST *ast);
static void compile_def(Compiler *compiler, AST *ast);
static void compile_return(Compiler *compiler, AST *ast);

static void compile_get_attr(Compiler *compiler, AST *ast);

static int max_stack_depth(byte *bc, size_t len);

static struct metadata compile_raw(Compiler *compiler, Program *program)
{
	fill_ct(compiler, program);
	write_sym_table(compiler);
	write_const_table(compiler);

	struct ast_list *ast_node = program;
	while (ast_node != NULL) {
		compile_node(compiler, ast_node->ast, true);
		ast_node = ast_node->next;
	}

	write_byte(compiler, INS_LOAD_NULL);
	write_byte(compiler, INS_RETURN);

	unsigned int max_vstack_depth = max_stack_depth(compiler->code.bc, compiler->code.size);
	struct metadata metadata;
	metadata.max_vstack_depth = max_vstack_depth;
	return metadata;
}

static struct metadata compile_program(Compiler *compiler, Program *program)
{
	populate_symtable(compiler->st, program);
	return compile_raw(compiler, program);
}

static void compile_const(Compiler *compiler, AST *ast)
{
	CTConst value;

	switch (ast->type) {
	case NODE_INT:
		value.type = CT_INT;
		value.value.i = ast->v.int_val;
		break;
	case NODE_FLOAT:
		value.type = CT_DOUBLE;
		value.value.d = ast->v.float_val;
		break;
	case NODE_STRING:
		value.type = CT_STRING;
		value.value.s = ast->v.str_val;
		break;
	case NODE_BLOCK: {
		/*
		 * a block indicates a function
		 */
		const unsigned int const_id = ct_poll_codeobj(compiler->ct);
		write_byte(compiler, INS_LOAD_CONST);
		write_uint16(compiler, const_id);
		return;
	}
	default:
		INTERNAL_ERROR();
	}

	const unsigned int const_id = ct_id_for_const(compiler->ct, value);
	write_byte(compiler, INS_LOAD_CONST);
	write_uint16(compiler, const_id);
}

static void compile_load(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_IDENT);

	const STSymbol *sym = ste_get_symbol(compiler->st->ste_current, ast->v.ident);

	if (sym == NULL) {
		INTERNAL_ERROR();
	}

	if (sym->bound_here) {
		write_byte(compiler, INS_LOAD);
	} else if (sym->global_var) {
		write_byte(compiler, INS_LOAD_GLOBAL);
	} else {
		/*
		 * TODO: non-global free variables
		 */
		assert(0);
	}

	write_uint16(compiler, sym->id);
}

static void compile_assignment(Compiler *compiler, AST *ast)
{
	const NodeType type = ast->type;
	if (!IS_ASSIGNMENT(type)) {
		INTERNAL_ERROR();
	}

	AST *lhs = ast->left;
	AST *rhs = ast->right;

	/*
	 *         (assign)
	 *        /       \
	 *       .        rhs
	 *      / \
	 *    id  attr
	 */
	if (lhs->type == NODE_DOT) {
		const STSymbol *sym = ste_get_attr_symbol(compiler->st->ste_current, lhs->right->v.ident);
		const unsigned int sym_id = sym->id;

		if (sym == NULL) {
			INTERNAL_ERROR();
		}

		if (type == NODE_ASSIGN) {
			compile_node(compiler, rhs, false);
			compile_node(compiler, lhs->left, false);
			write_byte(compiler, INS_SET_ATTR);
			write_uint16(compiler, sym_id);
		} else {
			compile_node(compiler, lhs->left, false);
			write_byte(compiler, INS_DUP);
			write_byte(compiler, INS_LOAD_ATTR);
			write_uint16(compiler, sym_id);
			compile_node(compiler, rhs, false);
			write_byte(compiler, to_opcode(type));
			write_byte(compiler, INS_ROT);
			write_byte(compiler, INS_SET_ATTR);
			write_uint16(compiler, sym_id);
		}
	} else {
		const STSymbol *sym = ste_get_symbol(compiler->st->ste_current, lhs->v.ident);
		const unsigned int sym_id = sym->id;

		if (sym == NULL) {
			INTERNAL_ERROR();
		}

		if (!sym->bound_here) {
			/*
			 * TODO: non-local assignments
			 */
			assert(0);
		}

		if (type == NODE_ASSIGN) {
			compile_node(compiler, rhs, false);
		} else {
			/* compound assignment */
			compile_load(compiler, lhs);
			compile_node(compiler, rhs, false);
			write_byte(compiler, to_opcode(type));
		}

		write_byte(compiler, INS_STORE);
		write_uint16(compiler, sym_id);
	}
}

static void compile_call(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_CALL);

	size_t argcount = 0;

	for (struct ast_list *node = ast->v.params; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, false);
		++argcount;
	}

	compile_node(compiler, ast->left, false);  // callable
	write_byte(compiler, INS_CALL);
	write_uint16(compiler, argcount);
}

static void compile_block(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_BLOCK);

	for (struct ast_list *node = ast->v.block; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, false);
	}
}

static void compile_if(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_IF);

	const bool has_else = (ast->v.middle != NULL);

	compile_node(compiler, ast->left, false);
	write_byte(compiler, INS_JMP_IF_FALSE);

	// jump placeholder:
	const size_t jmp_stub_idx = compiler->code.size;
	write_uint16(compiler, 0);
	// ~~~

	compile_node(compiler, ast->right, false);

	// fill in placeholder:
	if (has_else) {
		write_byte(compiler, INS_JMP);
		const size_t jmp_stub_idx_2 = compiler->code.size;
		write_uint16(compiler, 0);

		write_uint16_at(compiler, compiler->code.size - jmp_stub_idx - 2, jmp_stub_idx);

		compile_node(compiler, ast->v.middle, false);

		write_uint16_at(compiler, compiler->code.size - jmp_stub_idx_2 - 2, jmp_stub_idx_2);
	} else {
		write_uint16_at(compiler, compiler->code.size - jmp_stub_idx - 2, jmp_stub_idx);
	}
}

static void compile_while(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_WHILE);

	write_byte(compiler, INS_JMP);

	// unconditional jump placeholder:
	const size_t jmp_ucond_stub_idx = compiler->code.size;
	write_uint16(compiler, 0);
	// ~~~

	compile_node(compiler, ast->right, false);  // body

	// fill in placeholder:
	write_uint16_at(compiler, compiler->code.size - jmp_ucond_stub_idx - 2, jmp_ucond_stub_idx);

	compile_node(compiler, ast->left, false);   // condition

	write_byte(compiler, INS_JMP_BACK_IF_TRUE);
	write_uint16(compiler, compiler->code.size - jmp_ucond_stub_idx);
}

static void compile_def(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_DEF);

	/*
	 * A function definition is essentially the assignment of
	 * a code object to a variable.
	 */
	const STSymbol *sym = ste_get_symbol(compiler->st->ste_current, ast->left->v.ident);

	if (sym == NULL) {
		INTERNAL_ERROR();
	}

	compile_const(compiler, ast->right);
	write_byte(compiler, INS_STORE);
	write_uint16(compiler, sym->id);
}

static void compile_return(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_RETURN);
	compile_node(compiler, ast->left, false);
	write_byte(compiler, INS_RETURN);
}

static void compile_get_attr(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_DOT);

	Str *attr = ast->right->v.ident;
	STSymbol *attr_sym = ste_get_attr_symbol(compiler->st->ste_current, attr);

	compile_node(compiler, ast->left, false);
	write_byte(compiler, INS_LOAD_ATTR);
	write_uint16(compiler, attr_sym->id);
}

/*
 * Converts AST node type to corresponding (most relevant) opcode.
 * For compound-assignment types, converts to the corresponding
 * in-place binary opcode.
 */
static Opcode to_opcode(NodeType type)
{
	switch (type) {
	case NODE_ADD:
		return INS_ADD;
	case NODE_SUB:
		return INS_SUB;
	case NODE_MUL:
		return INS_MUL;
	case NODE_DIV:
		return INS_DIV;
	case NODE_MOD:
		return INS_MOD;
	case NODE_POW:
		return INS_POW;
	case NODE_BITAND:
		return INS_BITAND;
	case NODE_BITOR:
		return INS_BITOR;
	case NODE_XOR:
		return INS_XOR;
	case NODE_BITNOT:
		return INS_BITNOT;
	case NODE_SHIFTL:
		return INS_SHIFTL;
	case NODE_SHIFTR:
		return INS_SHIFTR;
	case NODE_AND:
		return INS_AND;
	case NODE_OR:
		return INS_OR;
	case NODE_NOT:
		return INS_NOT;
	case NODE_EQUAL:
		return INS_EQUAL;
	case NODE_NOTEQ:
		return INS_NOTEQ;
	case NODE_LT:
		return INS_LT;
	case NODE_GT:
		return INS_GT;
	case NODE_LE:
		return INS_LE;
	case NODE_GE:
		return INS_GE;
	case NODE_UPLUS:
		return INS_NOP;
	case NODE_UMINUS:
		return INS_UMINUS;
	case NODE_ASSIGN:
		return INS_STORE;
	case NODE_ASSIGN_ADD:
		return INS_IADD;
	case NODE_ASSIGN_SUB:
		return INS_ISUB;
	case NODE_ASSIGN_MUL:
		return INS_IMUL;
	case NODE_ASSIGN_DIV:
		return INS_IDIV;
	case NODE_ASSIGN_MOD:
		return INS_IMOD;
	case NODE_ASSIGN_POW:
		return INS_IPOW;
	case NODE_ASSIGN_BITAND:
		return INS_IBITAND;
	case NODE_ASSIGN_BITOR:
		return INS_IBITOR;
	case NODE_ASSIGN_XOR:
		return INS_IXOR;
	case NODE_ASSIGN_SHIFTL:
		return INS_ISHIFTL;
	case NODE_ASSIGN_SHIFTR:
		return INS_ISHIFTR;
	default:
		INTERNAL_ERROR();
		return 0;
	}
}

static void compile_node(Compiler *compiler, AST *ast, bool toplevel)
{
	if (ast == NULL) {
		return;
	}

	switch (ast->type) {
	case NODE_INT:
	case NODE_FLOAT:
	case NODE_STRING:
		compile_const(compiler, ast);
		break;
	case NODE_IDENT:
		compile_load(compiler, ast);
		break;
	case NODE_ADD:
	case NODE_SUB:
	case NODE_MUL:
	case NODE_DIV:
	case NODE_MOD:
	case NODE_POW:
	case NODE_BITAND:
	case NODE_BITOR:
	case NODE_XOR:
	case NODE_SHIFTL:
	case NODE_SHIFTR:
	case NODE_AND:
	case NODE_OR:
	case NODE_EQUAL:
	case NODE_NOTEQ:
	case NODE_LT:
	case NODE_GT:
	case NODE_LE:
	case NODE_GE:
		compile_node(compiler, ast->left, false);
		compile_node(compiler, ast->right, false);
		write_byte(compiler, to_opcode(ast->type));
		break;
	case NODE_DOT:
		compile_get_attr(compiler, ast);
		break;
	case NODE_ASSIGN:
	case NODE_ASSIGN_ADD:
	case NODE_ASSIGN_SUB:
	case NODE_ASSIGN_MUL:
	case NODE_ASSIGN_DIV:
	case NODE_ASSIGN_MOD:
	case NODE_ASSIGN_POW:
	case NODE_ASSIGN_BITAND:
	case NODE_ASSIGN_BITOR:
	case NODE_ASSIGN_XOR:
	case NODE_ASSIGN_SHIFTL:
	case NODE_ASSIGN_SHIFTR:
		compile_assignment(compiler, ast);
		break;
	case NODE_BITNOT:
	case NODE_NOT:
	case NODE_UPLUS:
	case NODE_UMINUS:
		compile_node(compiler, ast->left, false);
		write_byte(compiler, to_opcode(ast->type));
		break;
	case NODE_PRINT:
		compile_node(compiler, ast->left, false);
		write_byte(compiler, INS_PRINT);
		break;
	case NODE_IF:
		compile_if(compiler, ast);
		break;
	case NODE_WHILE:
		compile_while(compiler, ast);
		break;
	case NODE_DEF:
		compile_def(compiler, ast);
		break;
	case NODE_RETURN:
		compile_return(compiler, ast);
		break;
	case NODE_BLOCK:
		compile_block(compiler, ast);
		break;
	case NODE_CALL:
		compile_call(compiler, ast);
		if (toplevel) {
			write_byte(compiler, INS_POP);
		}
		break;
	default:
		INTERNAL_ERROR();
	}
}

/*
 * Symbol table format:
 *
 * - ST_ENTRY_BEGIN
 * - 4-byte int: no. of locals (N)
 * - N null-terminated strings representing local variable names
 *   ...
 * - 4-byte int: no. of attributes (M)
 * - M null-terminated strings representing attribute names
 *   ...
 * - ST_ENTRY_END
 */
static void write_sym_table(Compiler *compiler)
{
	const STEntry *ste = compiler->st->ste_current;
	const size_t n_locals = ste->n_locals;
	const size_t n_attrs = ste->attr_size;

	Str **locals_sorted = malloc(n_locals * sizeof(Str *));

	const size_t table_capacity = ste->table_capacity;

	for (size_t i = 0; i < table_capacity; i++) {
		for (STSymbol *e = ste->table[i]; e != NULL; e = e->next) {
			if (e->bound_here) {
				locals_sorted[e->id] = e->key;
			}
		}
	}

	Str **attrs_sorted = malloc(n_attrs * sizeof(Str *));

	const size_t attr_capacity = ste->attr_capacity;

	for (size_t i = 0; i < attr_capacity; i++) {
		for (STSymbol *e = ste->attributes[i]; e != NULL; e = e->next) {
			attrs_sorted[e->id] = e->key;
		}
	}

	code_write_byte(&compiler->code, ST_ENTRY_BEGIN);
	code_write_uint16(&compiler->code, n_locals);

	for (size_t i = 0; i < n_locals; i++) {
		code_write_str(&compiler->code, locals_sorted[i]);
	}

	code_write_uint16(&compiler->code, n_attrs);

	for (size_t i = 0; i < n_attrs; i++) {
		code_write_str(&compiler->code, attrs_sorted[i]);
	}

	code_write_byte(&compiler->code, ST_ENTRY_END);

	free(locals_sorted);
	free(attrs_sorted);
}

static void write_const_table(Compiler *compiler)
{
	const ConstTable *ct = compiler->ct;
	const size_t size = ct->table_size + ct->codeobjs_size;

	code_write_byte(&compiler->code, CT_ENTRY_BEGIN);
	code_write_uint16(&compiler->code, size);

	CTConst *sorted = malloc(size * sizeof(CTConst));

	const size_t capacity = ct->capacity;
	for (size_t i = 0; i < capacity; i++) {
		for (CTEntry *e = ct->table[i]; e != NULL; e = e->next) {
			sorted[e->value] = e->key;
		}
	}

	for (CTEntry *e = ct->codeobjs_head; e != NULL; e = e->next) {
		sorted[e->value] = e->key;
	}

	for (size_t i = 0; i < size; i++) {
		switch (sorted[i].type) {
		case CT_INT:
			code_write_byte(&compiler->code, CT_ENTRY_INT);
			code_write_int(&compiler->code, sorted[i].value.i);
			break;
		case CT_DOUBLE:
			code_write_byte(&compiler->code, CT_ENTRY_FLOAT);
			code_write_double(&compiler->code, sorted[i].value.d);
			break;
		case CT_STRING:
			code_write_byte(&compiler->code, CT_ENTRY_STRING);
			code_write_str(&compiler->code, sorted[i].value.s);
			break;
		case CT_CODEOBJ:
			code_write_byte(&compiler->code, CT_ENTRY_CODEOBJ);

			/*
			 * CodeObject bytecode begins with a name, followed by a
			 * 4-byte int indicating how many arguments it takes.
			 */

			Code *co_code = sorted[i].value.c;

			size_t name_len = 0;
			while (co_code->bc[name_len] != '\0') {
				++name_len;
			}

			/*
			 * Write size of actual CodeObject bytecode, excluding
			 * metadata (name, argcount, stack_depth):
			 */
			code_write_uint16(&compiler->code, co_code->size - (name_len + 1) - 2 - 2);

			code_append(&compiler->code, co_code);
			code_dealloc(co_code);
			free(co_code);
			break;
		}
	}
	free(sorted);

	code_write_byte(&compiler->code, CT_ENTRY_END);
}

static void fill_ct_from_ast(Compiler *compiler, AST *ast)
{
	if (ast == NULL) {
		return;
	}

	CTConst value;

	switch (ast->type) {
	case NODE_INT:
		value.type = CT_INT;
		value.value.i = ast->v.int_val;
		break;
	case NODE_FLOAT:
		value.type = CT_DOUBLE;
		value.value.d = ast->v.float_val;
		break;
	case NODE_STRING:
		value.type = CT_STRING;
		value.value.s = ast->v.str_val;
		break;
	case NODE_DEF: {
		value.type = CT_CODEOBJ;

		SymTable *st = compiler->st;
		STEntry *parent = compiler->st->ste_current;
		STEntry *child = parent->children[parent->child_pos++];

		unsigned int nargs = 0;
		ParamList *params = ast->v.params;

		while (params != NULL) {
			params = params->next;
			++nargs;
		}

		st->ste_current = child;
		Compiler *sub = compiler_new(compiler->filename, st);
		struct metadata metadata = compile_raw(sub, ast->right->v.block);
		st->ste_current = parent;

		Code *subcode = &sub->code;

		unsigned int max_vstack_depth = metadata.max_vstack_depth;

		Code *fncode = malloc(sizeof(Code));

		const size_t name_len = ast->left->v.ident->len;

		code_init(fncode, name_len + 2 + 2 + sub->code.size);  // total size
		code_write_str(fncode, ast->left->v.ident);            // name
		code_write_uint16(fncode, nargs);                      // arg count
		code_write_uint16(fncode, max_vstack_depth);           // max stack depth
		code_append(fncode, subcode);

		compiler_free(sub, false);
		value.value.c = fncode;
		break;
	}
	case NODE_IF:
		fill_ct_from_ast(compiler, ast->left);
		fill_ct_from_ast(compiler, ast->right);
		fill_ct_from_ast(compiler, ast->v.middle);
		return;
	case NODE_CALL:
		for (struct ast_list *node = ast->v.params; node != NULL; node = node->next) {
			fill_ct_from_ast(compiler, node->ast);
		}
		goto end;
	case NODE_BLOCK:
		for (struct ast_list *node = ast->v.block; node != NULL; node = node->next) {
			fill_ct_from_ast(compiler, node->ast);
		}
		goto end;
	default:
		goto end;
	}

	ct_id_for_const(compiler->ct, value);
	return;

	end:
	fill_ct_from_ast(compiler, ast->left);
	fill_ct_from_ast(compiler, ast->right);
}

static void fill_ct(Compiler *compiler, Program *program)
{
	for (struct ast_list *node = program; node != NULL; node = node->next) {
		fill_ct_from_ast(compiler, node->ast);
	}
}

static int stack_delta(Opcode opcode, int arg);
static int read_arg(Opcode opcode, byte **bc);
static int arg_size(Opcode opcode);

static int max_stack_depth(byte *bc, size_t len)
{
	const byte *end = bc + len;

	/*
	 * Skip over symbol table and
	 * constant table...
	 */

	if (*bc == ST_ENTRY_BEGIN) {
		++bc;  // ST_ENTRY_BEGIN
		const size_t n_locals = read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < n_locals; i++) {
			while (*bc++ != '\0');
		}

		const size_t n_attrs = read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < n_attrs; i++) {
			while (*bc++ != '\0');
		}

		++bc;  // ST_ENTRY_END
	}

	if (*bc == CT_ENTRY_BEGIN) {
		++bc;  // CT_ENTRY_BEGIN
		const size_t ct_size = read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < ct_size; i++) {
			switch (*bc++) {
			case CT_ENTRY_INT:
				bc += INT_SIZE;
				break;
			case CT_ENTRY_FLOAT:
				bc += DOUBLE_SIZE;
				break;
			case CT_ENTRY_STRING: {
				while (*bc++ != '\0');
				break;
			}
			case CT_ENTRY_CODEOBJ: {
				size_t colen = read_uint16_from_stream(bc);
				bc += 2;

				while (*bc++ != '\0');  // name
				bc += 2;  // arg count
				bc += 2;  // stack depth

				for (size_t i = 0; i < colen; i++) {
					++bc;
				}
				break;
			}
			}
		}

		++bc;  // CT_ENTRY_END
	}

	/*
	 * Begin max depth computation...
	 */

	int depth = 0;
	int max_depth = 0;

	while (bc != end) {
		byte opcode = *bc++;

		int arg = read_arg(opcode, &bc);
		int delta = stack_delta(opcode, arg);
		depth += delta;
		assert(depth >= 0);

		if (depth > max_depth) {
			max_depth = depth;
		}
	}

	return max_depth;
}

static int arg_size(Opcode opcode)
{
	switch (opcode) {
	case INS_NOP:
		return 0;
	case INS_LOAD_CONST:
		return 2;
	case INS_LOAD_NULL:
		return 0;
	case INS_ADD:
	case INS_SUB:
	case INS_MUL:
	case INS_DIV:
	case INS_MOD:
	case INS_POW:
	case INS_BITAND:
	case INS_BITOR:
	case INS_XOR:
	case INS_BITNOT:
	case INS_SHIFTL:
	case INS_SHIFTR:
	case INS_AND:
	case INS_OR:
	case INS_NOT:
	case INS_EQUAL:
	case INS_NOTEQ:
	case INS_LT:
	case INS_GT:
	case INS_LE:
	case INS_GE:
	case INS_UPLUS:
	case INS_UMINUS:
	case INS_IADD:
	case INS_ISUB:
	case INS_IMUL:
	case INS_IDIV:
	case INS_IMOD:
	case INS_IPOW:
	case INS_IBITAND:
	case INS_IBITOR:
	case INS_IXOR:
	case INS_ISHIFTL:
	case INS_ISHIFTR:
		return 0;
	case INS_STORE:
	case INS_LOAD:
	case INS_LOAD_GLOBAL:
	case INS_LOAD_ATTR:
	case INS_SET_ATTR:
		return 2;
	case INS_PRINT:
		return 0;
	case INS_JMP:
	case INS_JMP_BACK:
	case INS_JMP_IF_TRUE:
	case INS_JMP_IF_FALSE:
	case INS_JMP_BACK_IF_TRUE:
	case INS_JMP_BACK_IF_FALSE:
	case INS_CALL:
		return 2;
	case INS_RETURN:
	case INS_POP:
	case INS_DUP:
	case INS_ROT:
		return 0;
	default:
		INTERNAL_ERROR();
		return 0;
	}
}

static int read_arg(Opcode opcode, byte **bc)
{
	int size = arg_size(opcode);
	int arg;

	switch (size) {
	case 0:
		arg = 0;
		break;
	case 1:
		arg = *(*bc++);
		break;
	case 2:
		arg = read_uint16_from_stream(*bc);
		*bc += 2;
		break;
	default:
		INTERNAL_ERROR();
		arg = 0;
		break;
	}

	return arg;
}

/*
 * Calculates the value stack depth change resulting from
 * executing the given opcode with the given argument.
 *
 * The use of this function relies on the assumption that
 * individual statements, upon completion, leave the stack
 * depth unchanged (i.e. at 0).
 */
static int stack_delta(Opcode opcode, int arg)
{
	switch (opcode) {
	case INS_NOP:
		return 0;
	case INS_LOAD_CONST:
	case INS_LOAD_NULL:
		return 1;
	case INS_ADD:
	case INS_SUB:
	case INS_MUL:
	case INS_DIV:
	case INS_MOD:
	case INS_POW:
	case INS_BITAND:
	case INS_BITOR:
	case INS_XOR:
		return -1;
	case INS_BITNOT:
		return 0;
	case INS_SHIFTL:
	case INS_SHIFTR:
	case INS_AND:
	case INS_OR:
		return -1;
	case INS_NOT:
		return 0;
	case INS_EQUAL:
	case INS_NOTEQ:
	case INS_LT:
	case INS_GT:
	case INS_LE:
	case INS_GE:
		return -1;
	case INS_UPLUS:
	case INS_UMINUS:
		return 0;
	case INS_IADD:
	case INS_ISUB:
	case INS_IMUL:
	case INS_IDIV:
	case INS_IMOD:
	case INS_IPOW:
	case INS_IBITAND:
	case INS_IBITOR:
	case INS_IXOR:
	case INS_ISHIFTL:
	case INS_ISHIFTR:
		return -1;
	case INS_STORE:
		return -1;
	case INS_LOAD:
	case INS_LOAD_GLOBAL:
		return 1;
	case INS_LOAD_ATTR:
		return 0;
	case INS_SET_ATTR:
		return -2;
	case INS_PRINT:
		return -1;
	case INS_JMP:
	case INS_JMP_BACK:
		return 0;
	case INS_JMP_IF_TRUE:
	case INS_JMP_IF_FALSE:
	case INS_JMP_BACK_IF_TRUE:
	case INS_JMP_BACK_IF_FALSE:
		return -1;
	case INS_CALL:
		return -arg;
	case INS_RETURN:
		return -1;
	case INS_POP:
		return -1;
	case INS_DUP:
		return 1;
	case INS_ROT:
		return 0;
	}
}

void compile(FILE *src, FILE *out, const char *name)
{
	fseek(src, 0L, SEEK_END);
	size_t code_size = ftell(src);
	fseek(src, 0L, SEEK_SET);
	char *code = malloc(code_size + 1);
	code[code_size] = '\0';

	if (code == NULL) {
		abort();
	}

	fread(code, sizeof(char), code_size, src);

	Lexer *lex = lex_new(code, code_size, name);
	Program *program = parse(lex);
	Compiler *compiler = compiler_new(name, st_new(name));

	struct metadata metadata = compile_program(compiler, program);

	ast_list_free(program);

	/*
	 * Every rhoc file should start with the
	 * "magic" bytes:
	 */
	for (size_t i = 0; i < magic_size; i++) {
		fputc(magic[i], out);
	}

	/*
	 * Directly after the magic bytes, we write
	 * the maximum value stack depth at module
	 * level:
	 */
	byte buf[2];
	write_uint16_to_stream(buf, metadata.max_vstack_depth);
	for (size_t i = 0; i < sizeof(buf); i++) {
		fputc(buf[i], out);
	}

	/*
	 * And now we write the actual bytecode:
	 */
	fwrite(compiler->code.bc, 1, compiler->code.size, out);

	compiler_free(compiler, true);
	lex_free(lex);
	free(code);
}
