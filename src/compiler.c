#include <stdlib.h>
#include <stdio.h>
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

void write_int(Compiler *c, const int n)
{
	code_write_int(&c->code, n);
}

int read_int(byte *bytes)
{
	int n;
	memcpy(&n, bytes, INT_SIZE);
	return n;
}

void write_double(Compiler *c, const double d)
{
	code_write_double(&c->code, d);
}

double read_double(byte *bytes)
{
	double d;
	memcpy(&d, bytes, DOUBLE_SIZE);
	return d;
}

const byte magic[] = {0xFE, 0xED, 0xF0, 0x0D};
const size_t magic_size = sizeof(magic);

static void write_byte(Compiler *compiler, byte p)
{
	code_write_byte(&compiler->code, p);
}

/*
 * Compilation
 */
static void fill_ct(Compiler *compiler, Program *program);
static void fill_st(Compiler *compiler, Program *program);
static void write_sym_table(Compiler *compiler);
static void write_const_table(Compiler *compiler);

static Opcode to_opcode(NodeType type);

static Compiler *compiler_new(void)
{
	Compiler *compiler = malloc(sizeof(Compiler));
	code_init(&compiler->code, DEFAULT_BC_CAPACITY);
	compiler->st = st_new();
	compiler->ct = ct_new();

	return compiler;
}

/*
 * Note: this does not deallocate the compiler's code
 * field.
 */
static void compiler_free(Compiler *compiler)
{
	st_free(compiler->st);
	ct_free(compiler->ct);
	free(compiler);
}

static void compile_node(Compiler *compiler, AST *ast);
static void compile_program(Compiler *compiler, Program *program);
static void compile_magic(Compiler *compiler);

static void compile_load(Compiler *compiler, AST *ast);
static void compile_assignment(Compiler *compiler, AST *ast);

static void compile_block(Compiler *compiler, AST *ast);
static void compile_if(Compiler *compiler, AST *ast);
static void compile_while(Compiler *compiler, AST *ast);
static void compile_def(Compiler *compiler, AST *ast);

/*
 * Each compiled file should begin with the
 * "magic code".
 */
static void compile_magic(Compiler *compiler)
{
	for (size_t i = 0; i < magic_size; i++) {
		write_byte(compiler, magic[i]);
	}
}

static void compile_raw(Compiler *compiler, Program *program)
{
	fill_st(compiler, program);
	fill_ct(compiler, program);
	write_sym_table(compiler);
	write_const_table(compiler);

	struct ast_list *ast_node = program;
	while (ast_node != NULL) {
		compile_node(compiler, ast_node->ast);
		ast_node = ast_node->next;
	}

	write_byte(compiler, INS_RETURN);
}

static void compile_program(Compiler *compiler, Program *program)
{
	compile_magic(compiler);
	compile_raw(compiler, program);
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
		const byte const_id = ct_poll_codeobj(compiler->ct);
		write_byte(compiler, INS_LOAD_CONST);
		write_byte(compiler, const_id);
		return;
	}
	default:
		INTERNAL_ERROR();
	}

	const byte const_id = ct_id_for_const(compiler->ct, value);
	write_byte(compiler, INS_LOAD_CONST);
	write_byte(compiler, const_id);
}

static void compile_load(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_IDENT);

	write_byte(compiler, INS_LOAD);
	const unsigned int ref_id = id_for_var(compiler->st, ast->v.ident);
	write_int(compiler, ref_id);
}

static void compile_assignment(Compiler *compiler, AST *ast)
{
	const NodeType type = ast->type;
	if (!IS_ASSIGNMENT(type)) {
		INTERNAL_ERROR();
	}

	const unsigned int ref_id = id_for_var(compiler->st, ast->left->v.ident);

	if (type == NODE_ASSIGN) {
		compile_node(compiler, ast->right);
	} else {
		/* compound assignment */
		compile_load(compiler, ast->left);
		compile_node(compiler, ast->right);
		write_byte(compiler, to_opcode(type));
	}

	write_byte(compiler, INS_STORE);
	write_int(compiler, ref_id);
}

static void compile_block(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_BLOCK);

	for (struct ast_list *node = ast->v.block; node != NULL; node = node->next) {
		compile_node(compiler, node->ast);
	}
}

static void compile_if(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_IF);

	compile_node(compiler, ast->left);
	write_byte(compiler, INS_JMP_IF_FALSE);

	// jump placeholder:
	const unsigned int jmp_stub_idx = compiler->code.size;
	write_byte(compiler, INS_NOP);
	// ~~~

	compile_node(compiler, ast->right);

	// fill in placeholder:
	compiler->code.bc[jmp_stub_idx] = (byte)(compiler->code.size - jmp_stub_idx - 1);
}

static void compile_while(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_WHILE);

	write_byte(compiler, INS_JMP);

	// unconditional jump placeholder:
	const unsigned int jmp_ucond_stub_idx = compiler->code.size;
	write_byte(compiler, INS_NOP);
	// ~~~

	compile_node(compiler, ast->right);  // body

	// fill in placeholder:
	compiler->code.bc[jmp_ucond_stub_idx] = (byte)(compiler->code.size - jmp_ucond_stub_idx - 1);

	compile_node(compiler, ast->left);   // condition

	write_byte(compiler, INS_JMP_BACK_IF_TRUE);
	write_byte(compiler, compiler->code.size - jmp_ucond_stub_idx);
}

static void compile_def(Compiler *compiler, AST *ast)
{
	AST_TYPE_ASSERT(ast, NODE_DEF);

	/*
	 * A function definition is essentially the assignment of
	 * a code object to a variable.
	 */
	const unsigned int ref_id = id_for_var(compiler->st, ast->left->v.ident);
	compile_const(compiler, ast->right);
	write_byte(compiler, INS_STORE);
	write_int(compiler, ref_id);
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

static void compile_node(Compiler *compiler, AST *ast)
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
		compile_node(compiler, ast->left);
		compile_node(compiler, ast->right);
		write_byte(compiler, to_opcode(ast->type));
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
		compile_node(compiler, ast->left);
		write_byte(compiler, to_opcode(ast->type));
		break;
	case NODE_PRINT:
		compile_node(compiler, ast->left);
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
	case NODE_BLOCK:
		compile_block(compiler, ast);
		break;
	case NODE_CALL:
		compile_node(compiler, ast->left);
		write_byte(compiler, INS_CALL);
		break;
	default:
		INTERNAL_ERROR();
	}
}

static void write_sym_table(Compiler *compiler)
{
	const size_t size = compiler->st->size;

	code_write_byte(&compiler->code, ST_ENTRY_BEGIN);
	code_write_int(&compiler->code, size);

	Str **sorted = malloc(size * sizeof(Str *));

	const size_t capacity = compiler->st->capacity;
	for (size_t i = 0; i < capacity; i++) {
		for (STEntry *e = compiler->st->table[i]; e != NULL; e = e->next) {
			sorted[e->value] = e->key;
		}
	}

	for (size_t i = 0; i < size; i++) {
		code_write_string(&compiler->code, sorted[i]);
	}
	free(sorted);

	code_write_byte(&compiler->code, ST_ENTRY_END);
}

static void fill_st_from_ast(Compiler *compiler, AST *ast)
{
	if (ast == NULL) {
		return;
	}

	if (ast->type == NODE_IDENT) {
		id_for_var(compiler->st, ast->v.ident);
	} else if (ast->type == NODE_BLOCK) {
		for (struct ast_list *node = ast->v.block; node != NULL; node = node->next) {
			fill_st_from_ast(compiler, node->ast);
		}
	}

	fill_st_from_ast(compiler, ast->left);
	fill_st_from_ast(compiler, ast->right);
}

static void fill_st(Compiler *compiler, Program *program)
{
	for (struct ast_list *node = program; node != NULL; node = node->next) {
		fill_st_from_ast(compiler, node->ast);
	}
}

static void write_const_table(Compiler *compiler)
{
	const ConstTable *ct = compiler->ct;
	const size_t size = ct->table_size + ct->codeobjs_size;

	code_write_byte(&compiler->code, CT_ENTRY_BEGIN);
	code_write_int(&compiler->code, size);

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
			code_write_string(&compiler->code, sorted[i].value.s);
			break;
		case CT_CODEOBJ:
			code_write_byte(&compiler->code, CT_ENTRY_CODEOBJ);
			code_write_int(&compiler->code, sorted[i].value.c->size);
			code_append(&compiler->code, sorted[i].value.c);
			code_dealloc(sorted[i].value.c);
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
		Compiler *sub = compiler_new();
		compile_raw(sub, ast->right->v.block);
		Code *fncode = malloc(sizeof(Code));
		*fncode = sub->code;
		compiler_free(sub);
		value.value.c = fncode;
		break;
	}
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

void compile(FILE *src, FILE *out, const char *name)
{
	fseek(src, 0L, SEEK_END);
	unsigned long numbytes = ftell(src);
	fseek(src, 0L, SEEK_SET);
	char *code = malloc(numbytes);

	if (code == NULL) {
		abort();
	}

	fread(code, sizeof(char), numbytes, src);

	Lexer *lex = lex_new(code, numbytes, name);
	Program *program = parse(lex);
	Compiler *compiler = compiler_new();

	compile_program(compiler, program);

	ast_list_free(program);

	fwrite(compiler->code.bc, 1, compiler->code.size, out);

	compiler_free(compiler);
	lex_free(lex);
}
