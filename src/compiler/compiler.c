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
#include "util.h"
#include "compiler.h"

struct metadata {
	size_t bc_size;
	size_t lno_table_size;
	unsigned int max_vstack_depth;
	unsigned int max_try_catch_depth;
};

#define LBI_INIT_CAPACITY 5

static void write_byte(RhoCompiler *compiler, const byte p)
{
	rho_code_write_byte(&compiler->code, p);
}

static void write_ins(RhoCompiler *compiler, const RhoOpcode p, unsigned int lineno)
{
#define WB(p) rho_code_write_byte(&compiler->lno_table, p)

	const unsigned int curr_lineno = compiler->last_lineno;

	if (lineno > curr_lineno) {
		unsigned int ins_delta = compiler->last_ins_idx - compiler->first_ins_on_line_idx;
		unsigned int lineno_delta = lineno - curr_lineno;
		compiler->first_ins_on_line_idx = compiler->last_ins_idx;

		while (lineno_delta || ins_delta) {
			byte x = ins_delta < 0xff ? ins_delta : 0xff;
			byte y = lineno_delta < 0xff ? lineno_delta : 0xff;
			WB(x);
			WB(y);
			ins_delta -= x;
			lineno_delta -= y;
		}

		compiler->last_lineno = lineno;
	}

	++compiler->last_ins_idx;
	write_byte(compiler, p);

#undef WB
}

static void write_int(RhoCompiler *compiler, const int n)
{
	rho_code_write_int(&compiler->code, n);
}

static void write_uint16(RhoCompiler *compiler, const size_t n)
{
	rho_code_write_uint16(&compiler->code, n);
}

static void write_uint16_at(RhoCompiler *compiler, const size_t n, const size_t pos)
{
	rho_code_write_uint16_at(&compiler->code, n, pos);
}

static void write_double(RhoCompiler *compiler, const double d)
{
	rho_code_write_double(&compiler->code, d);
}

static void write_str(RhoCompiler *compiler, const RhoStr *str)
{
	rho_code_write_str(&compiler->code, str);
}

static void append(RhoCompiler *compiler, const RhoCode *code)
{
	rho_code_append(&compiler->code, code);
}

const byte rho_magic[] = {0xFE, 0xED, 0xF0, 0x0D};
const size_t rho_magic_size = sizeof(rho_magic);

/*
 * Compilation
 */
static void fill_ct(RhoCompiler *compiler, RhoProgram *program);
static void write_sym_table(RhoCompiler *compiler);
static void write_const_table(RhoCompiler *compiler);

static RhoOpcode to_opcode(RhoNodeType type);

#define DEFAULT_BC_CAPACITY        100
#define DEFAULT_LNO_TABLE_CAPACITY 30

static RhoCompiler *compiler_new(const char *filename, unsigned int first_lineno, RhoSymTable *st)
{
	RhoCompiler *compiler = rho_malloc(sizeof(RhoCompiler));
	compiler->filename = filename;
	rho_code_init(&compiler->code, DEFAULT_BC_CAPACITY);
	compiler->lbi = NULL;
	compiler->st = st;
	compiler->ct = rho_ct_new();
	compiler->try_catch_depth = 0;
	compiler->try_catch_depth_max = 0;
	rho_code_init(&compiler->lno_table, DEFAULT_LNO_TABLE_CAPACITY);
	compiler->first_lineno = first_lineno;
	compiler->first_ins_on_line_idx = 0;
	compiler->last_ins_idx = 0;
	compiler->last_lineno = first_lineno;
	compiler->in_generator = 0;

	return compiler;
}

/*
 * Note: this does not deallocate the compiler's code field.
 */
static void compiler_free(RhoCompiler *compiler, bool free_st)
{
	if (free_st) {
		rho_st_free(compiler->st);
	}
	rho_ct_free(compiler->ct);
	rho_code_dealloc(&compiler->code);
	rho_code_dealloc(&compiler->lno_table);
	free(compiler);
}

static struct rho_loop_block_info *lbi_new(size_t start_index, struct rho_loop_block_info *prev)
{
	struct rho_loop_block_info *lbi = rho_malloc(sizeof(*lbi));
	lbi->start_index = start_index;
	lbi->break_indices = rho_malloc(LBI_INIT_CAPACITY * sizeof(size_t));
	lbi->break_indices_size = 0;
	lbi->break_indices_capacity = LBI_INIT_CAPACITY;
	lbi->prev = prev;
	return lbi;
}

static void lbi_add_break_index(struct rho_loop_block_info *lbi, size_t break_index)
{
	size_t bi_size = lbi->break_indices_size;
	size_t bi_capacity = lbi->break_indices_capacity;

	if (bi_size == bi_capacity) {
		bi_capacity = (bi_capacity * 3)/2 + 1;
		lbi->break_indices = rho_realloc(lbi->break_indices, bi_capacity * sizeof(size_t));
		lbi->break_indices_capacity = bi_capacity;
	}

	lbi->break_indices[lbi->break_indices_size++] = break_index;
}

void lbi_free(struct rho_loop_block_info *lbi)
{
	free(lbi->break_indices);
	free(lbi);
}

static void compiler_push_loop(RhoCompiler *compiler, size_t start_index)
{
	compiler->lbi = lbi_new(start_index, compiler->lbi);
}

static void compiler_pop_loop(RhoCompiler *compiler)
{
	struct rho_loop_block_info *lbi = compiler->lbi;
	const size_t *break_indices = lbi->break_indices;
	const size_t break_indices_size = lbi->break_indices_size;
	const size_t end_index = compiler->code.size;

	for (size_t i = 0; i < break_indices_size; i++) {
		const size_t break_index = break_indices[i];
		write_uint16_at(compiler, end_index - break_index - 2, break_index);
	}

	compiler->lbi = lbi->prev;
	lbi_free(lbi);
}

static void compile_node(RhoCompiler *compiler, RhoAST *ast, bool toplevel);
static struct metadata compile_program(RhoCompiler *compiler, RhoProgram *program);

static void compile_load(RhoCompiler *compiler, RhoAST *ast);
static void compile_assignment(RhoCompiler *compiler, RhoAST *ast);

static void compile_and(RhoCompiler *compiler, RhoAST *ast);
static void compile_or(RhoCompiler *compiler, RhoAST *ast);

static void compile_call(RhoCompiler *compiler, RhoAST *ast);

static void compile_cond_expr(RhoCompiler *compiler, RhoAST *ast);

static void compile_block(RhoCompiler *compiler, RhoAST *ast);
static void compile_list(RhoCompiler *compiler, RhoAST *ast);
static void compile_tuple(RhoCompiler *compiler, RhoAST *ast);
static void compile_set(RhoCompiler *compiler, RhoAST *ast);
static void compile_dict(RhoCompiler *compiler, RhoAST *ast);
static void compile_dict_elem(RhoCompiler *compiler, RhoAST *ast);
static void compile_index(RhoCompiler *compiler, RhoAST *ast);

static void compile_if(RhoCompiler *compiler, RhoAST *ast);
static void compile_while(RhoCompiler *compiler, RhoAST *ast);
static void compile_for(RhoCompiler *compiler, RhoAST *ast);
static void compile_def(RhoCompiler *compiler, RhoAST *ast);
static void compile_gen(RhoCompiler *compiler, RhoAST *ast);
static void compile_act(RhoCompiler *compiler, RhoAST *ast);
static void compile_lambda(RhoCompiler *compiler, RhoAST *ast);
static void compile_break(RhoCompiler *compiler, RhoAST *ast);
static void compile_continue(RhoCompiler *compiler, RhoAST *ast);
static void compile_return(RhoCompiler *compiler, RhoAST *ast);
static void compile_throw(RhoCompiler *compiler, RhoAST *ast);
static void compile_produce(RhoCompiler *compiler, RhoAST *ast);
static void compile_receive(RhoCompiler *compiler, RhoAST *ast);
static void compile_try_catch(RhoCompiler *compiler, RhoAST *ast);
static void compile_import(RhoCompiler *compiler, RhoAST *ast);
static void compile_export(RhoCompiler *compiler, RhoAST *ast);

static void compile_get_attr(RhoCompiler *compiler, RhoAST *ast);

static int max_stack_depth(byte *bc, size_t len);

static struct metadata compile_raw(RhoCompiler *compiler, RhoProgram *program, bool is_single_expr)
{
	if (is_single_expr) {
		assert(program->next == NULL);
	}

	fill_ct(compiler, program);
	write_sym_table(compiler);
	write_const_table(compiler);

	const size_t start_size = compiler->code.size;

	for (struct rho_ast_list *node = program; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, !is_single_expr);
	}

	if (is_single_expr) {
		write_ins(compiler, RHO_INS_RETURN, 0);
	} else {
		write_ins(compiler,
		          compiler->in_generator ? RHO_INS_LOAD_ITER_STOP : RHO_INS_LOAD_NULL,
		          0);
		write_ins(compiler, RHO_INS_RETURN, 0);
	}

	RhoCode *code = &compiler->code;
	RhoCode *lno_table = &compiler->lno_table;

	/* two zeros mark the end of the line number table */
	rho_code_write_byte(lno_table, 0);
	rho_code_write_byte(lno_table, 0);

	const size_t final_size = code->size;
	const size_t bc_size = final_size - start_size;
	unsigned int max_vstack_depth = max_stack_depth(code->bc, code->size);
	unsigned int max_try_catch_depth = compiler->try_catch_depth_max;

	/*
	 * What follows is somewhat delicate: we want the line number
	 * table to come before the symbol/constant tables in the compiled
	 * code, but we do not have a completed line number table until
	 * compilation is complete, so we copy everything into a new Code
	 * instance and use that as our finished product.
	 */
	const size_t lno_table_size = lno_table->size;
	RhoCode complete;
	rho_code_init(&complete, 2 + 2 + lno_table_size + final_size);
	rho_code_write_uint16(&complete, compiler->first_lineno);
	rho_code_write_uint16(&complete, lno_table_size);
	rho_code_append(&complete, lno_table);
	rho_code_append(&complete, code);
	rho_code_dealloc(code);
	compiler->code = complete;

	/* return some data describing what we compiled */
	struct metadata metadata;
	metadata.bc_size = bc_size;
	metadata.lno_table_size = lno_table_size;
	metadata.max_vstack_depth = max_vstack_depth;
	metadata.max_try_catch_depth = max_try_catch_depth;
	return metadata;
}

static struct metadata compile_program(RhoCompiler *compiler, RhoProgram *program)
{
	rho_st_populate(compiler->st, program);
	return compile_raw(compiler, program, false);
}

static void compile_const(RhoCompiler *compiler, RhoAST *ast)
{
	const unsigned int lineno = ast->lineno;
	RhoCTConst value;

	switch (ast->type) {
	case RHO_NODE_INT:
		value.type = RHO_CT_INT;
		value.value.i = ast->v.int_val;
		break;
	case RHO_NODE_FLOAT:
		value.type = RHO_CT_DOUBLE;
		value.value.d = ast->v.float_val;
		break;
	case RHO_NODE_STRING:
		value.type = RHO_CT_STRING;
		value.value.s = ast->v.str_val;
		break;
	case RHO_NODE_DEF:
	case RHO_NODE_GEN:
	case RHO_NODE_ACT:
	case RHO_NODE_LAMBDA: {
		const unsigned int const_id = rho_ct_poll_codeobj(compiler->ct);
		write_ins(compiler, RHO_INS_LOAD_CONST, lineno);
		write_uint16(compiler, const_id);
		return;
	}
	default:
		RHO_INTERNAL_ERROR();
	}

	const unsigned int const_id = rho_ct_id_for_const(compiler->ct, value);
	write_ins(compiler, RHO_INS_LOAD_CONST, lineno);
	write_uint16(compiler, const_id);
}

static void compile_load(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_IDENT);

	const unsigned int lineno = ast->lineno;
	const RhoSTSymbol *sym = rho_ste_get_symbol(compiler->st->ste_current, ast->v.ident);

	if (sym == NULL) {
		RHO_INTERNAL_ERROR();
	}

	if (sym->bound_here) {
		write_ins(compiler, RHO_INS_LOAD, lineno);
	} else if (sym->global_var) {
		write_ins(compiler, RHO_INS_LOAD_GLOBAL, lineno);
	} else {
		assert(sym->free_var);
		write_ins(compiler, RHO_INS_LOAD_NAME, lineno);
	}

	write_uint16(compiler, sym->id);
}

static void compile_assignment(RhoCompiler *compiler, RhoAST *ast)
{
	const RhoNodeType type = ast->type;
	if (!RHO_NODE_TYPE_IS_ASSIGNMENT(type)) {
		RHO_INTERNAL_ERROR();
	}

	const unsigned int lineno = ast->lineno;

	RhoAST *lhs = ast->left;
	RhoAST *rhs = ast->right;

	/*
	 *         (assign)
	 *        /       \
	 *       .        rhs
	 *      / \
	 *    id  attr
	 */
	if (lhs->type == RHO_NODE_DOT) {
		const RhoSTSymbol *sym = rho_ste_get_attr_symbol(compiler->st->ste_current, lhs->right->v.ident);
		const unsigned int sym_id = sym->id;

		if (sym == NULL) {
			RHO_INTERNAL_ERROR();
		}

		if (type == RHO_NODE_ASSIGN) {
			compile_node(compiler, rhs, false);
			compile_node(compiler, lhs->left, false);
			write_ins(compiler, RHO_INS_SET_ATTR, lineno);
			write_uint16(compiler, sym_id);
		} else {
			/* compound assignment */
			compile_node(compiler, lhs->left, false);
			write_ins(compiler, RHO_INS_DUP, lineno);
			write_ins(compiler, RHO_INS_LOAD_ATTR, lineno);
			write_uint16(compiler, sym_id);
			compile_node(compiler, rhs, false);
			write_ins(compiler, to_opcode(type), lineno);
			write_ins(compiler, RHO_INS_ROT, lineno);
			write_ins(compiler, RHO_INS_SET_ATTR, lineno);
			write_uint16(compiler, sym_id);
		}
	} else if (lhs->type == RHO_NODE_INDEX) {
		if (type == RHO_NODE_ASSIGN) {
			compile_node(compiler, rhs, false);
			compile_node(compiler, lhs->left, false);
			compile_node(compiler, lhs->right, false);
			write_ins(compiler, RHO_INS_SET_INDEX, lineno);
		} else {
			/* compound assignment */
			compile_node(compiler, lhs->left, false);
			compile_node(compiler, lhs->right, false);
			write_ins(compiler, RHO_INS_DUP_TWO, lineno);
			write_ins(compiler, RHO_INS_LOAD_INDEX, lineno);
			compile_node(compiler, rhs, false);
			write_ins(compiler, to_opcode(type), lineno);
			write_ins(compiler, RHO_INS_ROT_THREE, lineno);
			write_ins(compiler, RHO_INS_SET_INDEX, lineno);
		}
	} else {
		const RhoSTSymbol *sym = rho_ste_get_symbol(compiler->st->ste_current, lhs->v.ident);

		if (sym == NULL) {
			RHO_INTERNAL_ERROR();
		}

		const unsigned int sym_id = sym->id;
		assert(sym->bound_here || sym->global_var);

		if (type == RHO_NODE_ASSIGN) {
			compile_node(compiler, rhs, false);
		} else {
			/* compound assignment */
			compile_load(compiler, lhs);
			compile_node(compiler, rhs, false);
			write_ins(compiler, to_opcode(type), lineno);
		}

		byte store_ins;
		if (sym->bound_here) {
			store_ins = RHO_INS_STORE;
		} else if (sym->global_var) {
			store_ins = RHO_INS_STORE_GLOBAL;
		} else {
			RHO_INTERNAL_ERROR();
		}

		write_ins(compiler, store_ins, lineno);
		write_uint16(compiler, sym_id);
	}
}

static void compile_call(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_CALL);

	const unsigned int lineno = ast->lineno;

	unsigned int unnamed_args = 0;
	unsigned int named_args = 0;

	bool flip = false;  // sanity check: no unnamed args after named ones
	for (struct rho_ast_list *node = ast->v.params; node != NULL; node = node->next) {
		if (RHO_NODE_TYPE_IS_ASSIGNMENT(node->ast->type)) {
			RHO_AST_TYPE_ASSERT(node->ast, RHO_NODE_ASSIGN);
			RHO_AST_TYPE_ASSERT(node->ast->left, RHO_NODE_IDENT);
			flip = true;

			RhoCTConst name;
			name.type = RHO_CT_STRING;
			name.value.s = node->ast->left->v.ident;
			const unsigned int id = rho_ct_id_for_const(compiler->ct, name);

			write_ins(compiler, RHO_INS_LOAD_CONST, lineno);
			write_uint16(compiler, id);
			compile_node(compiler, node->ast->right, false);

			++named_args;
		} else {
			assert(!flip);
			compile_node(compiler, node->ast, false);

			++unnamed_args;
		}
	}

	assert(unnamed_args <= 0xff && named_args <= 0xff);

	compile_node(compiler, ast->left, false);  // callable
	write_ins(compiler, RHO_INS_CALL, lineno);
	write_uint16(compiler, (named_args << 8) | unnamed_args);
}

static void compile_cond_expr(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_COND_EXPR);

	const unsigned int lineno = ast->lineno;

	compile_node(compiler, ast->v.middle, false);  // condition
	write_ins(compiler, RHO_INS_JMP_IF_FALSE, lineno);
	const size_t jmp_to_false_index = compiler->code.size;
	write_uint16(compiler, 0);

	compile_node(compiler, ast->left, false);  // true branch
	write_ins(compiler, RHO_INS_JMP, lineno);
	const size_t jmp_out_index = compiler->code.size;
	write_uint16(compiler, 0);

	write_uint16_at(compiler, compiler->code.size - jmp_to_false_index - 2, jmp_to_false_index);

	compile_node(compiler, ast->right, false);  // false branch

	write_uint16_at(compiler, compiler->code.size - jmp_out_index - 2, jmp_out_index);
}

static void compile_and(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_AND);
	compile_node(compiler, ast->left, false);
	write_ins(compiler, RHO_INS_JMP_IF_FALSE_ELSE_POP, ast->left->lineno);
	size_t jump_index = compiler->code.size;
	write_uint16(compiler, 0);  // placeholder for jump offset
	compile_node(compiler, ast->right, false);
	write_uint16_at(compiler, compiler->code.size - jump_index - 2, jump_index);
}

static void compile_or(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_OR);
	compile_node(compiler, ast->left, false);
	write_ins(compiler, RHO_INS_JMP_IF_TRUE_ELSE_POP, ast->left->lineno);
	size_t jump_index = compiler->code.size;
	write_uint16(compiler, 0);  // placeholder for jump offset
	compile_node(compiler, ast->right, false);
	write_uint16_at(compiler, compiler->code.size - jump_index - 2, jump_index);
}


static void compile_block(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_BLOCK);

	for (struct rho_ast_list *node = ast->v.block; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, true);
	}
}

static void compile_list(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_LIST);

	size_t len = 0;
	for (struct rho_ast_list *node = ast->v.list; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, false);
		++len;
	}

	write_ins(compiler, RHO_INS_MAKE_LIST, ast->lineno);
	write_uint16(compiler, len);
}

static void compile_tuple(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_TUPLE);

	size_t len = 0;
	for (struct rho_ast_list *node = ast->v.list; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, false);
		++len;
	}

	write_ins(compiler, RHO_INS_MAKE_TUPLE, ast->lineno);
	write_uint16(compiler, len);
}

static void compile_set(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_SET);

	size_t len = 0;
	for (struct rho_ast_list *node = ast->v.list; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, false);
		++len;
	}

	write_ins(compiler, RHO_INS_MAKE_SET, ast->lineno);
	write_uint16(compiler, len);
}

static void compile_dict(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_DICT);

	size_t len = 0;
	for (struct rho_ast_list *node = ast->v.list; node != NULL; node = node->next) {
		RHO_AST_TYPE_ASSERT(node->ast, RHO_NODE_DICT_ELEM);
		compile_node(compiler, node->ast, false);
		len += 2;
	}

	write_ins(compiler, RHO_INS_MAKE_DICT, ast->lineno);
	write_uint16(compiler, len);
}

static void compile_dict_elem(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_DICT_ELEM);
	compile_node(compiler, ast->left, false);
	compile_node(compiler, ast->right, false);
}

static void compile_index(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_INDEX);
	compile_node(compiler, ast->left, false);
	compile_node(compiler, ast->right, false);
	write_ins(compiler, RHO_INS_LOAD_INDEX, ast->lineno);
}

static void compile_if(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_IF);

	RhoAST *else_chain_base = ast->v.middle;
	unsigned int n_elifs = 0;

	for (RhoAST *node = else_chain_base; node != NULL; node = node->v.middle) {
		if (node->type == RHO_NODE_ELSE) {
			assert(node->v.middle == NULL);   // this'd better be the last node
		} else {
			assert(node->type == RHO_NODE_ELIF);  // only ELSE and ELIF nodes allowed here
			++n_elifs;
		}
	}

	/*
	 * Placeholder indices for jump offsets following the ELSE/ELIF bodies.
	 */
	size_t *jmp_placeholder_indices = rho_malloc((1 + n_elifs) * sizeof(size_t));
	size_t node_index = 0;

	for (RhoAST *node = ast; node != NULL; node = node->v.middle) {
		RhoNodeType type = node->type;
		const unsigned int lineno = node->lineno;

		switch (type) {
		case RHO_NODE_IF:
		case RHO_NODE_ELIF: {
			compile_node(compiler, node->left, false);  // condition
			write_ins(compiler, RHO_INS_JMP_IF_FALSE, lineno);
			const size_t jmp_to_next_index = compiler->code.size;
			write_uint16(compiler, 0);

			compile_node(compiler, node->right, true);  // body
			write_ins(compiler, RHO_INS_JMP, lineno);
			const size_t jmp_out_index = compiler->code.size;
			write_uint16(compiler, 0);

			jmp_placeholder_indices[node_index++] = jmp_out_index;
			write_uint16_at(compiler, compiler->code.size - jmp_to_next_index - 2, jmp_to_next_index);
			break;
		}
		case RHO_NODE_ELSE: {
			compile_node(compiler, node->left, true);  // body
			break;
		}
		default:
			RHO_INTERNAL_ERROR();
		}
	}

	const size_t final_size = compiler->code.size;

	for (size_t i = 0; i <= n_elifs; i++) {
		const size_t jmp_placeholder_index = jmp_placeholder_indices[i];
		write_uint16_at(compiler, final_size - jmp_placeholder_index - 2, jmp_placeholder_index);
	}

	free(jmp_placeholder_indices);
}

static void compile_while(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_WHILE);

	const size_t loop_start_index = compiler->code.size;
	compile_node(compiler, ast->left, false);  // condition
	write_ins(compiler, RHO_INS_JMP_IF_FALSE, 0);

	// jump placeholder:
	const size_t jump_index = compiler->code.size;
	write_uint16(compiler, 0);

	compiler_push_loop(compiler, loop_start_index);
	compile_node(compiler, ast->right, true);  // body

	write_ins(compiler, RHO_INS_JMP_BACK, 0);
	write_uint16(compiler, compiler->code.size - loop_start_index + 2);

	// fill in placeholder:
	write_uint16_at(compiler, compiler->code.size - jump_index - 2, jump_index);

	compiler_pop_loop(compiler);
}

static void compile_for(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_FOR);
	const unsigned int lineno = ast->lineno;

	RhoAST *lcv = ast->left;  // loop control variable
	RhoAST *iter = ast->right;
	RhoAST *body = ast->v.middle;

	compile_node(compiler, iter, false);
	write_ins(compiler, RHO_INS_GET_ITER, lineno);

	const size_t loop_start_index = compiler->code.size;
	compiler_push_loop(compiler, loop_start_index);
	write_ins(compiler, RHO_INS_LOOP_ITER, iter->lineno);

	// jump placeholder:
	const size_t jump_index = compiler->code.size;
	write_uint16(compiler, 0);

	if (lcv->type == RHO_NODE_IDENT) {
		const RhoSTSymbol *sym = rho_ste_get_symbol(compiler->st->ste_current, lcv->v.ident);

		if (sym == NULL) {
			RHO_INTERNAL_ERROR();
		}

		write_ins(compiler, RHO_INS_STORE, lineno);
		write_uint16(compiler, sym->id);
	} else {
		RHO_AST_TYPE_ASSERT(lcv, RHO_NODE_TUPLE);

		write_ins(compiler, RHO_INS_SEQ_EXPAND, lcv->lineno);

		unsigned int count = 0;
		for (struct rho_ast_list *node = lcv->v.list; node != NULL; node = node->next) {
			++count;
		}

		write_uint16(compiler, count);

		/* sequence is expanded left-to-right, so we have to store in reverse */
		for (int i = count-1; i >= 0; i--) {
			struct rho_ast_list *node = lcv->v.list;
			for (int j = 0; j < i; j++) {
				node = node->next;
			}

			RHO_AST_TYPE_ASSERT(node->ast, RHO_NODE_IDENT);
			const RhoSTSymbol *sym = rho_ste_get_symbol(compiler->st->ste_current, node->ast->v.ident);

			if (sym == NULL) {
				RHO_INTERNAL_ERROR();
			}

			write_ins(compiler, RHO_INS_STORE, lineno);
			write_uint16(compiler, sym->id);
		}
	}

	compile_node(compiler, body, true);

	write_ins(compiler, RHO_INS_JMP_BACK, 0);
	write_uint16(compiler, compiler->code.size - loop_start_index + 2);

	// fill in placeholder:
	write_uint16_at(compiler, compiler->code.size - jump_index - 2, jump_index);

	compiler_pop_loop(compiler);

	write_ins(compiler, RHO_INS_POP, 0);  // pop the iterator left behind by GET_ITER
}

#define COMPILE_DEF 0
#define COMPILE_GEN 1
#define COMPILE_ACT 2

static void compile_def_or_gen_or_act(RhoCompiler *compiler, RhoAST *ast, const int select)
{
	switch (select) {
	case COMPILE_DEF:
		RHO_AST_TYPE_ASSERT(ast, RHO_NODE_DEF);
		break;
	case COMPILE_GEN:
		RHO_AST_TYPE_ASSERT(ast, RHO_NODE_GEN);
		break;
	case COMPILE_ACT:
		RHO_AST_TYPE_ASSERT(ast, RHO_NODE_ACT);
		break;
	default:
		RHO_INTERNAL_ERROR();
	}

	const unsigned int lineno = ast->lineno;
	const RhoAST *name = ast->left;

	/* A function definition is essentially the assignment of a CodeObject to a variable. */
	const RhoSTSymbol *sym = rho_ste_get_symbol(compiler->st->ste_current, name->v.ident);

	if (sym == NULL) {
		RHO_INTERNAL_ERROR();
	}

	compile_const(compiler, ast);

	/* type hints */
	unsigned int num_hints = 0;
	for (struct rho_ast_list *param = ast->v.params; param != NULL; param = param->next) {
		RhoAST *v = param->ast;

		if (v->type == RHO_NODE_ASSIGN) {
			v = v->left;
		}

		RHO_AST_TYPE_ASSERT(v, RHO_NODE_IDENT);

		if (v->left != NULL) {
			compile_load(compiler, v->left);
		} else {
			write_ins(compiler, RHO_INS_LOAD_NULL, lineno);
		}

		++num_hints;
	}

	if (name->left != NULL) {
		RHO_AST_TYPE_ASSERT(name->left, RHO_NODE_IDENT);
		compile_load(compiler, name->left);
	} else {
		write_ins(compiler, RHO_INS_LOAD_NULL, lineno);
	}

	++num_hints;

	/* defaults */
	bool flip = false;  // sanity check: no non-default args after default ones
	unsigned int num_defaults = 0;
	for (struct rho_ast_list *param = ast->v.params; param != NULL; param = param->next) {
		if (param->ast->type == RHO_NODE_ASSIGN) {
			flip = true;
			RHO_AST_TYPE_ASSERT(param->ast->left, RHO_NODE_IDENT);
			compile_node(compiler, param->ast->right, false);
			++num_defaults;
		} else {
			assert(!flip);
		}
	}

	assert(num_defaults <= 0xff);
	assert(num_hints <= 0xff);

	switch (select) {
	case COMPILE_DEF:
		write_ins(compiler, RHO_INS_MAKE_FUNCOBJ, lineno);
		break;
	case COMPILE_GEN:
		write_ins(compiler, RHO_INS_MAKE_GENERATOR, lineno);
		break;
	case COMPILE_ACT:
		write_ins(compiler, RHO_INS_MAKE_ACTOR, lineno);
		break;
	default:
		RHO_INTERNAL_ERROR();
	}

	write_uint16(compiler, (num_hints << 8) | num_defaults);

	write_ins(compiler, RHO_INS_STORE, lineno);
	write_uint16(compiler, sym->id);
}

static void compile_def(RhoCompiler *compiler, RhoAST *ast)
{
	compile_def_or_gen_or_act(compiler, ast, COMPILE_DEF);
}

static void compile_gen(RhoCompiler *compiler, RhoAST *ast)
{
	compile_def_or_gen_or_act(compiler, ast, COMPILE_GEN);
}

static void compile_act(RhoCompiler *compiler, RhoAST *ast)
{
	compile_def_or_gen_or_act(compiler, ast, COMPILE_ACT);
}

#undef COMPILE_DEF
#undef COMPILE_GEN
#undef COMPILE_ACT

static void compile_lambda(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_LAMBDA);
	compile_const(compiler, ast);
	write_ins(compiler, RHO_INS_MAKE_FUNCOBJ, ast->lineno);
	write_uint16(compiler, 0);
}

static void compile_break(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_BREAK);
	const unsigned int lineno = ast->lineno;

	if (compiler->lbi == NULL) {
		RHO_INTERNAL_ERROR();
	}

	write_ins(compiler, RHO_INS_JMP, lineno);
	const size_t break_index = compiler->code.size;
	write_uint16(compiler, 0);

	/*
	 * We don't know where to jump to until we finish compiling
	 * the entire loop, so we keep a list of "breaks" and fill
	 * in their jumps afterwards.
	 */
	lbi_add_break_index(compiler->lbi, break_index);
}

static void compile_continue(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_CONTINUE);
	const unsigned int lineno = ast->lineno;

	if (compiler->lbi == NULL) {
		RHO_INTERNAL_ERROR();
	}

	write_ins(compiler, RHO_INS_JMP_BACK, lineno);
	const size_t start_index = compiler->lbi->start_index;
	write_uint16(compiler, compiler->code.size - start_index + 2);
}

static void compile_return(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_RETURN);
	const unsigned int lineno = ast->lineno;

	if (ast->left != NULL) {
		compile_node(compiler, ast->left, false);
	} else {
		write_ins(compiler,
		          compiler->in_generator ? RHO_INS_LOAD_ITER_STOP : RHO_INS_LOAD_NULL,
		          lineno);
	}

	write_ins(compiler, RHO_INS_RETURN, lineno);
}

static void compile_throw(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_THROW);
	const unsigned int lineno = ast->lineno;
	compile_node(compiler, ast->left, false);
	write_ins(compiler, RHO_INS_THROW, lineno);
}

static void compile_produce(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_PRODUCE);
	const unsigned int lineno = ast->lineno;
	compile_node(compiler, ast->left, false);
	write_ins(compiler, RHO_INS_PRODUCE, lineno);
}

static void compile_receive(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_RECEIVE);
	const unsigned int lineno = ast->lineno;
	const RhoSTSymbol *sym = rho_ste_get_symbol(compiler->st->ste_current, ast->left->v.ident);

	if (sym == NULL || !sym->bound_here) {
		RHO_INTERNAL_ERROR();
	}

	write_ins(compiler, RHO_INS_RECEIVE, lineno);
	write_ins(compiler, RHO_INS_STORE, lineno);
	write_uint16(compiler, sym->id);
}

static void compile_try_catch(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_TRY_CATCH);
	const unsigned int try_lineno = ast->lineno;
	const unsigned int catch_lineno = ast->right->lineno;

	unsigned int exc_count = 0;
	for (struct rho_ast_list *node = ast->v.excs; node != NULL; node = node->next) {
		++exc_count;
	}
	assert(exc_count > 0);
	assert(exc_count == 1);  // TODO: handle 2+ exceptions (this is currently valid syntactically)

	/* === Try Block === */
	write_ins(compiler, RHO_INS_TRY_BEGIN, try_lineno);
	const size_t try_block_size_index = compiler->code.size;
	write_uint16(compiler, 0);  /* placeholder for try-length */
	const size_t handler_offset_index = compiler->code.size;
	write_uint16(compiler, 0);  /* placeholder for handler offset */

	compiler->try_catch_depth += exc_count;

	if (compiler->try_catch_depth > compiler->try_catch_depth_max) {
		compiler->try_catch_depth_max = compiler->try_catch_depth;
	}

	compile_node(compiler, ast->left, true);  /* try block */
	compiler->try_catch_depth -= exc_count;

	write_ins(compiler, RHO_INS_TRY_END, catch_lineno);
	write_uint16_at(compiler, compiler->code.size - try_block_size_index - 4, try_block_size_index);

	write_ins(compiler, RHO_INS_JMP, catch_lineno);  /* jump past exception handlers if no exception was thrown */
	const size_t jmp_over_handlers_index = compiler->code.size;
	write_uint16(compiler, 0);  /* placeholder for jump offset */

	write_uint16_at(compiler, compiler->code.size - handler_offset_index - 2, handler_offset_index);

	/* === Handler === */
	write_ins(compiler, RHO_INS_DUP, catch_lineno);
	compile_node(compiler, ast->v.excs->ast, false);
	write_ins(compiler, RHO_INS_JMP_IF_EXC_MISMATCH, catch_lineno);
	const size_t exc_mismatch_jmp_index = compiler->code.size;
	write_uint16(compiler, 0);  /* placeholder for jump offset */

	write_ins(compiler, RHO_INS_POP, catch_lineno);
	compile_node(compiler, ast->right, true);  /* catch */

	/* jump over re-throw */
	write_ins(compiler, RHO_INS_JMP, catch_lineno);
	write_uint16(compiler, 1);

	write_uint16_at(compiler, compiler->code.size - exc_mismatch_jmp_index - 2, exc_mismatch_jmp_index);

	write_ins(compiler, RHO_INS_THROW, catch_lineno);

	write_uint16_at(compiler, compiler->code.size - jmp_over_handlers_index - 2, jmp_over_handlers_index);
}

static void compile_import(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_IMPORT);

	ast = ast->left;

	const unsigned int lineno = ast->lineno;
	const RhoSTSymbol *sym = rho_ste_get_symbol(compiler->st->ste_current, ast->v.ident);

	if (sym == NULL) {
		RHO_INTERNAL_ERROR();
	}

	const unsigned int sym_id = sym->id;

	write_ins(compiler, RHO_INS_IMPORT, lineno);
	write_uint16(compiler, sym_id);
	write_ins(compiler, RHO_INS_STORE, lineno);
	write_uint16(compiler, sym_id);
}

static void compile_export(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_EXPORT);

	compile_load(compiler, ast->left);

	ast = ast->left;

	const unsigned int lineno = ast->lineno;
	const RhoSTSymbol *sym = rho_ste_get_symbol(compiler->st->ste_current, ast->v.ident);

	if (sym == NULL) {
		RHO_INTERNAL_ERROR();
	}

	if (sym->bound_here) {
		write_ins(compiler, RHO_INS_EXPORT, lineno);
	} else if (sym->global_var) {
		write_ins(compiler, RHO_INS_EXPORT_GLOBAL, lineno);
	} else {
		assert(sym->free_var);
		write_ins(compiler, RHO_INS_EXPORT_NAME, lineno);
	}

	write_uint16(compiler, sym->id);
}

static void compile_get_attr(RhoCompiler *compiler, RhoAST *ast)
{
	RHO_AST_TYPE_ASSERT(ast, RHO_NODE_DOT);
	const unsigned int lineno = ast->lineno;

	RhoStr *attr = ast->right->v.ident;
	RhoSTSymbol *attr_sym = rho_ste_get_attr_symbol(compiler->st->ste_current, attr);

	compile_node(compiler, ast->left, false);
	write_ins(compiler, RHO_INS_LOAD_ATTR, lineno);
	write_uint16(compiler, attr_sym->id);
}

/*
 * Converts AST node type to corresponding (most relevant) opcode.
 * For compound-assignment types, converts to the corresponding
 * in-place binary opcode.
 */
static RhoOpcode to_opcode(RhoNodeType type)
{
	switch (type) {
	case RHO_NODE_ADD:
		return RHO_INS_ADD;
	case RHO_NODE_SUB:
		return RHO_INS_SUB;
	case RHO_NODE_MUL:
		return RHO_INS_MUL;
	case RHO_NODE_DIV:
		return RHO_INS_DIV;
	case RHO_NODE_MOD:
		return RHO_INS_MOD;
	case RHO_NODE_POW:
		return RHO_INS_POW;
	case RHO_NODE_BITAND:
		return RHO_INS_BITAND;
	case RHO_NODE_BITOR:
		return RHO_INS_BITOR;
	case RHO_NODE_XOR:
		return RHO_INS_XOR;
	case RHO_NODE_BITNOT:
		return RHO_INS_BITNOT;
	case RHO_NODE_SHIFTL:
		return RHO_INS_SHIFTL;
	case RHO_NODE_SHIFTR:
		return RHO_INS_SHIFTR;
	case RHO_NODE_AND:
		return RHO_INS_AND;
	case RHO_NODE_OR:
		return RHO_INS_OR;
	case RHO_NODE_NOT:
		return RHO_INS_NOT;
	case RHO_NODE_EQUAL:
		return RHO_INS_EQUAL;
	case RHO_NODE_NOTEQ:
		return RHO_INS_NOTEQ;
	case RHO_NODE_LT:
		return RHO_INS_LT;
	case RHO_NODE_GT:
		return RHO_INS_GT;
	case RHO_NODE_LE:
		return RHO_INS_LE;
	case RHO_NODE_GE:
		return RHO_INS_GE;
	case RHO_NODE_APPLY:
		return RHO_INS_APPLY;
	case RHO_NODE_UPLUS:
		return RHO_INS_NOP;
	case RHO_NODE_UMINUS:
		return RHO_INS_UMINUS;
	case RHO_NODE_ASSIGN:
		return RHO_INS_STORE;
	case RHO_NODE_ASSIGN_ADD:
		return RHO_INS_IADD;
	case RHO_NODE_ASSIGN_SUB:
		return RHO_INS_ISUB;
	case RHO_NODE_ASSIGN_MUL:
		return RHO_INS_IMUL;
	case RHO_NODE_ASSIGN_DIV:
		return RHO_INS_IDIV;
	case RHO_NODE_ASSIGN_MOD:
		return RHO_INS_IMOD;
	case RHO_NODE_ASSIGN_POW:
		return RHO_INS_IPOW;
	case RHO_NODE_ASSIGN_BITAND:
		return RHO_INS_IBITAND;
	case RHO_NODE_ASSIGN_BITOR:
		return RHO_INS_IBITOR;
	case RHO_NODE_ASSIGN_XOR:
		return RHO_INS_IXOR;
	case RHO_NODE_ASSIGN_SHIFTL:
		return RHO_INS_ISHIFTL;
	case RHO_NODE_ASSIGN_SHIFTR:
		return RHO_INS_ISHIFTR;
	case RHO_NODE_ASSIGN_APPLY:
		return RHO_INS_IAPPLY;
	case RHO_NODE_IN:
		return RHO_INS_IN;
	case RHO_NODE_DOTDOT:
		return RHO_INS_MAKE_RANGE;
	default:
		RHO_INTERNAL_ERROR();
		return 0;
	}
}

static void compile_node(RhoCompiler *compiler, RhoAST *ast, bool toplevel)
{
	if (ast == NULL) {
		return;
	}

	const unsigned int lineno = ast->lineno;

	switch (ast->type) {
	case RHO_NODE_NULL:
		write_ins(compiler, RHO_INS_LOAD_NULL, lineno);
		break;
	case RHO_NODE_INT:
	case RHO_NODE_FLOAT:
	case RHO_NODE_STRING:
		compile_const(compiler, ast);
		break;
	case RHO_NODE_IDENT:
		compile_load(compiler, ast);
		break;
	case RHO_NODE_ADD:
	case RHO_NODE_SUB:
	case RHO_NODE_MUL:
	case RHO_NODE_DIV:
	case RHO_NODE_MOD:
	case RHO_NODE_POW:
	case RHO_NODE_BITAND:
	case RHO_NODE_BITOR:
	case RHO_NODE_XOR:
	case RHO_NODE_SHIFTL:
	case RHO_NODE_SHIFTR:
	case RHO_NODE_EQUAL:
	case RHO_NODE_NOTEQ:
	case RHO_NODE_LT:
	case RHO_NODE_GT:
	case RHO_NODE_LE:
	case RHO_NODE_GE:
	case RHO_NODE_APPLY:
	case RHO_NODE_DOTDOT:
	case RHO_NODE_IN:
		compile_node(compiler, ast->left, false);
		compile_node(compiler, ast->right, false);
		write_ins(compiler, to_opcode(ast->type), lineno);
		break;
	case RHO_NODE_AND:
		compile_and(compiler, ast);
		break;
	case RHO_NODE_OR:
		compile_or(compiler, ast);
		break;
	case RHO_NODE_DOT:
		compile_get_attr(compiler, ast);
		break;
	case RHO_NODE_ASSIGN:
	case RHO_NODE_ASSIGN_ADD:
	case RHO_NODE_ASSIGN_SUB:
	case RHO_NODE_ASSIGN_MUL:
	case RHO_NODE_ASSIGN_DIV:
	case RHO_NODE_ASSIGN_MOD:
	case RHO_NODE_ASSIGN_POW:
	case RHO_NODE_ASSIGN_BITAND:
	case RHO_NODE_ASSIGN_BITOR:
	case RHO_NODE_ASSIGN_XOR:
	case RHO_NODE_ASSIGN_SHIFTL:
	case RHO_NODE_ASSIGN_SHIFTR:
	case RHO_NODE_ASSIGN_APPLY:
		compile_assignment(compiler, ast);
		break;
	case RHO_NODE_BITNOT:
	case RHO_NODE_NOT:
	case RHO_NODE_UPLUS:
	case RHO_NODE_UMINUS:
		compile_node(compiler, ast->left, false);
		write_ins(compiler, to_opcode(ast->type), lineno);
		break;
	case RHO_NODE_COND_EXPR:
		compile_cond_expr(compiler, ast);
		break;
	case RHO_NODE_PRINT:
		compile_node(compiler, ast->left, false);
		write_ins(compiler, RHO_INS_PRINT, lineno);
		break;
	case RHO_NODE_IF:
		compile_if(compiler, ast);
		break;
	case RHO_NODE_WHILE:
		compile_while(compiler, ast);
		break;
	case RHO_NODE_FOR:
		compile_for(compiler, ast);
		break;
	case RHO_NODE_DEF:
		compile_def(compiler, ast);
		break;
	case RHO_NODE_GEN:
		compile_gen(compiler, ast);
		break;
	case RHO_NODE_ACT:
		compile_act(compiler, ast);
		break;
	case RHO_NODE_LAMBDA:
		compile_lambda(compiler, ast);
		break;
	case RHO_NODE_BREAK:
		compile_break(compiler, ast);
		break;
	case RHO_NODE_CONTINUE:
		compile_continue(compiler, ast);
		break;
	case RHO_NODE_RETURN:
		compile_return(compiler, ast);
		break;
	case RHO_NODE_THROW:
		compile_throw(compiler, ast);
		break;
	case RHO_NODE_PRODUCE:
		compile_produce(compiler, ast);
		break;
	case RHO_NODE_RECEIVE:
		compile_receive(compiler, ast);
		break;
	case RHO_NODE_TRY_CATCH:
		compile_try_catch(compiler, ast);
		break;
	case RHO_NODE_IMPORT:
		compile_import(compiler, ast);
		break;
	case RHO_NODE_EXPORT:
		compile_export(compiler, ast);
		break;
	case RHO_NODE_BLOCK:
		compile_block(compiler, ast);
		break;
	case RHO_NODE_LIST:
		compile_list(compiler, ast);
		break;
	case RHO_NODE_TUPLE:
		compile_tuple(compiler, ast);
		break;
	case RHO_NODE_SET:
		compile_set(compiler, ast);
		break;
	case RHO_NODE_DICT:
		compile_dict(compiler, ast);
		break;
	case RHO_NODE_DICT_ELEM:
		compile_dict_elem(compiler, ast);
		break;
	case RHO_NODE_CALL:
		compile_call(compiler, ast);
		if (toplevel) {
			write_ins(compiler, RHO_INS_POP, lineno);
		}
		break;
	case RHO_NODE_INDEX:
		compile_index(compiler, ast);
		break;
	default:
		RHO_INTERNAL_ERROR();
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
static void write_sym_table(RhoCompiler *compiler)
{
	const RhoSTEntry *ste = compiler->st->ste_current;
	const size_t n_locals = ste->next_local_id;
	const size_t n_attrs = ste->next_attr_id;
	const size_t n_free = ste->next_free_var_id;

	RhoStr **locals_sorted = rho_malloc(n_locals * sizeof(RhoStr *));
	RhoStr **frees_sorted = rho_malloc(n_free * sizeof(RhoStr *));

	const size_t table_capacity = ste->table_capacity;

	for (size_t i = 0; i < table_capacity; i++) {
		for (RhoSTSymbol *e = ste->table[i]; e != NULL; e = e->next) {
			if (e->bound_here) {
				locals_sorted[e->id] = e->key;
			} else if (e->free_var) {
				frees_sorted[e->id] = e->key;
			}
		}
	}

	RhoStr **attrs_sorted = rho_malloc(n_attrs * sizeof(RhoStr *));

	const size_t attr_capacity = ste->attr_capacity;

	for (size_t i = 0; i < attr_capacity; i++) {
		for (RhoSTSymbol *e = ste->attributes[i]; e != NULL; e = e->next) {
			attrs_sorted[e->id] = e->key;
		}
	}

	write_byte(compiler, RHO_ST_ENTRY_BEGIN);

	write_uint16(compiler, n_locals);
	for (size_t i = 0; i < n_locals; i++) {
		write_str(compiler, locals_sorted[i]);
	}

	write_uint16(compiler, n_attrs);
	for (size_t i = 0; i < n_attrs; i++) {
		write_str(compiler, attrs_sorted[i]);
	}

	write_uint16(compiler, n_free);
	for (size_t i = 0; i < n_free; i++) {
		write_str(compiler, frees_sorted[i]);
	}

	write_byte(compiler, RHO_ST_ENTRY_END);

	free(locals_sorted);
	free(frees_sorted);
	free(attrs_sorted);
}

static void write_const_table(RhoCompiler *compiler)
{
	const RhoConstTable *ct = compiler->ct;
	const size_t size = ct->table_size + ct->codeobjs_size;

	write_byte(compiler, RHO_CT_ENTRY_BEGIN);
	write_uint16(compiler, size);

	RhoCTConst *sorted = rho_malloc(size * sizeof(RhoCTConst));

	const size_t capacity = ct->capacity;
	for (size_t i = 0; i < capacity; i++) {
		for (RhoCTEntry *e = ct->table[i]; e != NULL; e = e->next) {
			sorted[e->value] = e->key;
		}
	}

	for (RhoCTEntry *e = ct->codeobjs_head; e != NULL; e = e->next) {
		sorted[e->value] = e->key;
	}

	for (size_t i = 0; i < size; i++) {
		switch (sorted[i].type) {
		case RHO_CT_INT:
			write_byte(compiler, RHO_CT_ENTRY_INT);
			write_int(compiler, sorted[i].value.i);
			break;
		case RHO_CT_DOUBLE:
			write_byte(compiler, RHO_CT_ENTRY_FLOAT);
			write_double(compiler, sorted[i].value.d);
			break;
		case RHO_CT_STRING:
			write_byte(compiler, RHO_CT_ENTRY_STRING);
			write_str(compiler, sorted[i].value.s);
			break;
		case RHO_CT_CODEOBJ:
			write_byte(compiler, RHO_CT_ENTRY_CODEOBJ);

			RhoCode *co_code = sorted[i].value.c;

			size_t name_len = 0;
			while (co_code->bc[name_len] != '\0') {
				++name_len;
			}

			/*
			 * Write size of actual CodeObject bytecode, excluding
			 * metadata (name, argcount, stack_depth, try_catch_depth):
			 */
			write_uint16(compiler, co_code->size - (name_len + 1) - 2 - 2 - 2);

			append(compiler, co_code);
			rho_code_dealloc(co_code);
			free(co_code);
			break;
		}
	}
	free(sorted);

	write_byte(compiler, RHO_CT_ENTRY_END);
}

static void fill_ct_from_ast(RhoCompiler *compiler, RhoAST *ast)
{
	if (ast == NULL) {
		return;
	}

	RhoCTConst value;

	switch (ast->type) {
	case RHO_NODE_INT:
		value.type = RHO_CT_INT;
		value.value.i = ast->v.int_val;
		break;
	case RHO_NODE_FLOAT:
		value.type = RHO_CT_DOUBLE;
		value.value.d = ast->v.float_val;
		break;
	case RHO_NODE_STRING:
		value.type = RHO_CT_STRING;
		value.value.s = ast->v.str_val;
		break;
	case RHO_NODE_DEF:
	case RHO_NODE_GEN:
	case RHO_NODE_ACT:
	case RHO_NODE_LAMBDA: {
		value.type = RHO_CT_CODEOBJ;

		RhoSymTable *st = compiler->st;
		RhoSTEntry *parent = compiler->st->ste_current;
		RhoSTEntry *child = parent->children[parent->child_pos++];

		unsigned int nargs;

		const bool def_or_gen_or_act =
		        (ast->type == RHO_NODE_DEF || ast->type == RHO_NODE_GEN || ast->type == RHO_NODE_ACT);

		if (def_or_gen_or_act) {
			nargs = 0;
			for (struct rho_ast_list *param = ast->v.params; param != NULL; param = param->next) {
				if (param->ast->type == RHO_NODE_ASSIGN) {
					fill_ct_from_ast(compiler, param->ast->right);
				}
				++nargs;
			}
		} else {
			nargs = ast->v.max_dollar_ident;
		}

		RhoBlock *body;

		if (def_or_gen_or_act) {
			body = ast->right->v.block;
		} else {
			body = rho_ast_list_new();
			body->ast = ast->left;
		}

		unsigned int lineno;
		if (body == NULL) {
			lineno = ast->right->lineno;
		} else {
			lineno = body->ast->lineno;
		}

		st->ste_current = child;
		RhoCompiler *sub = compiler_new(compiler->filename, lineno, st);
		if (ast->type == RHO_NODE_GEN) {
			sub->in_generator = 1;
		}

		struct metadata metadata = compile_raw(sub, body, (ast->type == RHO_NODE_LAMBDA));
		st->ste_current = parent;

		RhoCode *subcode = &sub->code;

		const unsigned int max_vstack_depth = metadata.max_vstack_depth;
		const unsigned int max_try_catch_depth = metadata.max_try_catch_depth;

		RhoCode *fncode = rho_malloc(sizeof(RhoCode));

#define LAMBDA "<lambda>"
		RhoStr name = (def_or_gen_or_act ? *ast->left->v.ident : RHO_STR_INIT(LAMBDA, strlen(LAMBDA), 0));
#undef LAMBDA

		rho_code_init(fncode, (name.len + 1) + 2 + 2 + 2 + subcode->size);  // total size
		rho_code_write_str(fncode, &name);                                  // name
		rho_code_write_uint16(fncode, nargs);                               // argument count
		rho_code_write_uint16(fncode, max_vstack_depth);                    // max stack depth
		rho_code_write_uint16(fncode, max_try_catch_depth);                 // max try-catch depth
		rho_code_append(fncode, subcode);

		compiler_free(sub, false);
		value.value.c = fncode;

		if (!def_or_gen_or_act) {
			free(body);
		}

		break;
	}
	case RHO_NODE_IF:
		fill_ct_from_ast(compiler, ast->left);
		fill_ct_from_ast(compiler, ast->right);

		for (RhoAST *node = ast->v.middle; node != NULL; node = node->v.middle) {
			fill_ct_from_ast(compiler, node);
		}
		return;
	case RHO_NODE_FOR:
		fill_ct_from_ast(compiler, ast->v.middle);
		goto end;
	case RHO_NODE_BLOCK:
		for (struct rho_ast_list *node = ast->v.block; node != NULL; node = node->next) {
			fill_ct_from_ast(compiler, node->ast);
		}
		goto end;
	case RHO_NODE_LIST:
	case RHO_NODE_TUPLE:
	case RHO_NODE_SET:
	case RHO_NODE_DICT:
		for (struct rho_ast_list *node = ast->v.list; node != NULL; node = node->next) {
			fill_ct_from_ast(compiler, node->ast);
		}
		goto end;
	case RHO_NODE_CALL:
		for (struct rho_ast_list *node = ast->v.params; node != NULL; node = node->next) {
			RhoAST *ast = node->ast;
			if (ast->type == RHO_NODE_ASSIGN) {
				RHO_AST_TYPE_ASSERT(ast->left, RHO_NODE_IDENT);
				value.type = RHO_CT_STRING;
				value.value.s = ast->left->v.ident;
				rho_ct_id_for_const(compiler->ct, value);
				fill_ct_from_ast(compiler, ast->right);
			} else {
				fill_ct_from_ast(compiler, ast);
			}
		}
		goto end;
	case RHO_NODE_TRY_CATCH:
		for (struct rho_ast_list *node = ast->v.excs; node != NULL; node = node->next) {
			fill_ct_from_ast(compiler, node->ast);
		}
		goto end;
	case RHO_NODE_COND_EXPR:
		fill_ct_from_ast(compiler, ast->v.middle);
		goto end;
	default:
		goto end;
	}

	rho_ct_id_for_const(compiler->ct, value);
	return;

	end:
	fill_ct_from_ast(compiler, ast->left);
	fill_ct_from_ast(compiler, ast->right);
}

static void fill_ct(RhoCompiler *compiler, RhoProgram *program)
{
	for (struct rho_ast_list *node = program; node != NULL; node = node->next) {
		fill_ct_from_ast(compiler, node->ast);
	}
}

static int stack_delta(RhoOpcode opcode, int arg);
static int read_arg(RhoOpcode opcode, byte **bc);

static int max_stack_depth(byte *bc, size_t len)
{
	const byte *end = bc + len;

	/*
	 * Skip over symbol table and
	 * constant table...
	 */

	if (*bc == RHO_ST_ENTRY_BEGIN) {
		++bc;  // ST_ENTRY_BEGIN
		const size_t n_locals = rho_util_read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < n_locals; i++) {
			while (*bc++ != '\0');
		}

		const size_t n_attrs = rho_util_read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < n_attrs; i++) {
			while (*bc++ != '\0');
		}

		const size_t n_frees = rho_util_read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < n_frees; i++) {
			while (*bc++ != '\0');
		}

		++bc;  // ST_ENTRY_END
	}

	if (*bc == RHO_CT_ENTRY_BEGIN) {
		++bc;  // CT_ENTRY_BEGIN
		const size_t ct_size = rho_util_read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < ct_size; i++) {
			switch (*bc++) {
			case RHO_CT_ENTRY_INT:
				bc += RHO_INT_SIZE;
				break;
			case RHO_CT_ENTRY_FLOAT:
				bc += RHO_DOUBLE_SIZE;
				break;
			case RHO_CT_ENTRY_STRING: {
				while (*bc++ != '\0');
				break;
			}
			case RHO_CT_ENTRY_CODEOBJ: {
				size_t colen = rho_util_read_uint16_from_stream(bc);
				bc += 2;

				while (*bc++ != '\0');  // name
				bc += 2;  // arg count
				bc += 2;  // stack depth
				bc += 2;  // try-catch depth

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
		if (depth < 0) {
			depth = 0;
		}

		if (depth > max_depth) {
			max_depth = depth;
		}
	}

	return max_depth;
}

int rho_opcode_arg_size(RhoOpcode opcode)
{
	switch (opcode) {
	case RHO_INS_NOP:
		return 0;
	case RHO_INS_LOAD_CONST:
		return 2;
	case RHO_INS_LOAD_NULL:
	case RHO_INS_LOAD_ITER_STOP:
		return 0;
	case RHO_INS_ADD:
	case RHO_INS_SUB:
	case RHO_INS_MUL:
	case RHO_INS_DIV:
	case RHO_INS_MOD:
	case RHO_INS_POW:
	case RHO_INS_BITAND:
	case RHO_INS_BITOR:
	case RHO_INS_XOR:
	case RHO_INS_BITNOT:
	case RHO_INS_SHIFTL:
	case RHO_INS_SHIFTR:
	case RHO_INS_AND:
	case RHO_INS_OR:
	case RHO_INS_NOT:
	case RHO_INS_EQUAL:
	case RHO_INS_NOTEQ:
	case RHO_INS_LT:
	case RHO_INS_GT:
	case RHO_INS_LE:
	case RHO_INS_GE:
	case RHO_INS_UPLUS:
	case RHO_INS_UMINUS:
	case RHO_INS_IADD:
	case RHO_INS_ISUB:
	case RHO_INS_IMUL:
	case RHO_INS_IDIV:
	case RHO_INS_IMOD:
	case RHO_INS_IPOW:
	case RHO_INS_IBITAND:
	case RHO_INS_IBITOR:
	case RHO_INS_IXOR:
	case RHO_INS_ISHIFTL:
	case RHO_INS_ISHIFTR:
	case RHO_INS_MAKE_RANGE:
	case RHO_INS_IN:
		return 0;
	case RHO_INS_STORE:
	case RHO_INS_STORE_GLOBAL:
	case RHO_INS_LOAD:
	case RHO_INS_LOAD_GLOBAL:
	case RHO_INS_LOAD_ATTR:
	case RHO_INS_SET_ATTR:
		return 2;
	case RHO_INS_LOAD_INDEX:
	case RHO_INS_SET_INDEX:
	case RHO_INS_APPLY:
	case RHO_INS_IAPPLY:
		return 0;
	case RHO_INS_LOAD_NAME:
		return 2;
	case RHO_INS_PRINT:
		return 0;
	case RHO_INS_JMP:
	case RHO_INS_JMP_BACK:
	case RHO_INS_JMP_IF_TRUE:
	case RHO_INS_JMP_IF_FALSE:
	case RHO_INS_JMP_BACK_IF_TRUE:
	case RHO_INS_JMP_BACK_IF_FALSE:
	case RHO_INS_JMP_IF_TRUE_ELSE_POP:
	case RHO_INS_JMP_IF_FALSE_ELSE_POP:
	case RHO_INS_CALL:
		return 2;
	case RHO_INS_RETURN:
	case RHO_INS_THROW:
	case RHO_INS_PRODUCE:
		return 0;
	case RHO_INS_TRY_BEGIN:
		return 4;
	case RHO_INS_TRY_END:
		return 0;
	case RHO_INS_JMP_IF_EXC_MISMATCH:
		return 2;
	case RHO_INS_MAKE_LIST:
	case RHO_INS_MAKE_TUPLE:
	case RHO_INS_MAKE_SET:
	case RHO_INS_MAKE_DICT:
		return 2;
	case RHO_INS_IMPORT:
	case RHO_INS_EXPORT:
	case RHO_INS_EXPORT_GLOBAL:
	case RHO_INS_EXPORT_NAME:
		return 2;
	case RHO_INS_RECEIVE:
		return 0;
	case RHO_INS_GET_ITER:
		return 0;
	case RHO_INS_LOOP_ITER:
		return 2;
	case RHO_INS_MAKE_FUNCOBJ:
	case RHO_INS_MAKE_GENERATOR:
	case RHO_INS_MAKE_ACTOR:
		return 2;
	case RHO_INS_SEQ_EXPAND:
		return 2;
	case RHO_INS_POP:
	case RHO_INS_DUP:
	case RHO_INS_DUP_TWO:
	case RHO_INS_ROT:
	case RHO_INS_ROT_THREE:
		return 0;
	default:
		return -1;
	}
}

static int read_arg(RhoOpcode opcode, byte **bc)
{
	int size = rho_opcode_arg_size(opcode);

	if (size < 0) {
		RHO_INTERNAL_ERROR();
	}

	int arg;

	switch (size) {
	case 0:
		arg = 0;
		break;
	case 1:
		arg = *(*bc++);
		break;
	case 2:
	case 4:  /* only return the first 2 bytes */
		arg = rho_util_read_uint16_from_stream(*bc);
		*bc += size;
		break;
	default:
		RHO_INTERNAL_ERROR();
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
static int stack_delta(RhoOpcode opcode, int arg)
{
	switch (opcode) {
	case RHO_INS_NOP:
		return 0;
	case RHO_INS_LOAD_CONST:
	case RHO_INS_LOAD_NULL:
	case RHO_INS_LOAD_ITER_STOP:
		return 1;
	case RHO_INS_ADD:
	case RHO_INS_SUB:
	case RHO_INS_MUL:
	case RHO_INS_DIV:
	case RHO_INS_MOD:
	case RHO_INS_POW:
	case RHO_INS_BITAND:
	case RHO_INS_BITOR:
	case RHO_INS_XOR:
		return -1;
	case RHO_INS_BITNOT:
		return 0;
	case RHO_INS_SHIFTL:
	case RHO_INS_SHIFTR:
	case RHO_INS_AND:
	case RHO_INS_OR:
		return -1;
	case RHO_INS_NOT:
		return 0;
	case RHO_INS_EQUAL:
	case RHO_INS_NOTEQ:
	case RHO_INS_LT:
	case RHO_INS_GT:
	case RHO_INS_LE:
	case RHO_INS_GE:
		return -1;
	case RHO_INS_UPLUS:
	case RHO_INS_UMINUS:
		return 0;
	case RHO_INS_IADD:
	case RHO_INS_ISUB:
	case RHO_INS_IMUL:
	case RHO_INS_IDIV:
	case RHO_INS_IMOD:
	case RHO_INS_IPOW:
	case RHO_INS_IBITAND:
	case RHO_INS_IBITOR:
	case RHO_INS_IXOR:
	case RHO_INS_ISHIFTL:
	case RHO_INS_ISHIFTR:
	case RHO_INS_MAKE_RANGE:
	case RHO_INS_IN:
		return -1;
	case RHO_INS_STORE:
	case RHO_INS_STORE_GLOBAL:
		return -1;
	case RHO_INS_LOAD:
	case RHO_INS_LOAD_GLOBAL:
		return 1;
	case RHO_INS_LOAD_ATTR:
		return 0;
	case RHO_INS_SET_ATTR:
		return -2;
	case RHO_INS_LOAD_INDEX:
		return -1;
	case RHO_INS_SET_INDEX:
		return -3;
	case RHO_INS_APPLY:
	case RHO_INS_IAPPLY:
		return -1;
	case RHO_INS_LOAD_NAME:
		return 1;
	case RHO_INS_PRINT:
		return -1;
	case RHO_INS_JMP:
	case RHO_INS_JMP_BACK:
		return 0;
	case RHO_INS_JMP_IF_TRUE:
	case RHO_INS_JMP_IF_FALSE:
	case RHO_INS_JMP_BACK_IF_TRUE:
	case RHO_INS_JMP_BACK_IF_FALSE:
		return -1;
	case RHO_INS_JMP_IF_TRUE_ELSE_POP:
	case RHO_INS_JMP_IF_FALSE_ELSE_POP:
		return 0;  // -1 if jump not taken
	case RHO_INS_CALL:
		return -((arg & 0xff) + 2*(arg >> 8));
	case RHO_INS_RETURN:
	case RHO_INS_THROW:
	case RHO_INS_PRODUCE:
		return -1;
	case RHO_INS_TRY_BEGIN:
		return 0;
	case RHO_INS_TRY_END:
		return 1;  // technically 0 but exception should be on stack before reaching handlers
	case RHO_INS_JMP_IF_EXC_MISMATCH:
		return -2;
	case RHO_INS_MAKE_LIST:
	case RHO_INS_MAKE_TUPLE:
	case RHO_INS_MAKE_SET:
	case RHO_INS_MAKE_DICT:
		return -arg + 1;
	case RHO_INS_IMPORT:
		return 1;
	case RHO_INS_EXPORT:
	case RHO_INS_EXPORT_GLOBAL:
	case RHO_INS_EXPORT_NAME:
		return -1;
	case RHO_INS_RECEIVE:
		return 1;
	case RHO_INS_GET_ITER:
		return 0;
	case RHO_INS_LOOP_ITER:
		return 1;
	case RHO_INS_MAKE_FUNCOBJ:
	case RHO_INS_MAKE_GENERATOR:
	case RHO_INS_MAKE_ACTOR:
		return -((arg & 0xff) + (arg >> 8));
	case RHO_INS_SEQ_EXPAND:
		return -1 + arg;
	case RHO_INS_POP:
		return -1;
	case RHO_INS_DUP:
		return 1;
	case RHO_INS_DUP_TWO:
		return 2;
	case RHO_INS_ROT:
	case RHO_INS_ROT_THREE:
		return 0;
	}

	RHO_INTERNAL_ERROR();
	return 0;
}

void rho_compile(const char *name, RhoProgram *prog, FILE *out)
{
	RhoCompiler *compiler = compiler_new(name, 1, rho_st_new(name));

	struct metadata metadata = compile_program(compiler, prog);

	/*
	 * Every rhoc file should start with the
	 * "magic" bytes:
	 */
	for (size_t i = 0; i < rho_magic_size; i++) {
		fputc(rho_magic[i], out);
	}

	/*
	 * Directly after the magic bytes, we write
	 * the maximum value stack depth at module
	 * level, followed by the maximum try-catch
	 * depth:
	 */
	byte buf[2];

	rho_util_write_uint16_to_stream(buf, metadata.max_vstack_depth);
	for (size_t i = 0; i < sizeof(buf); i++) {
		fputc(buf[i], out);
	}

	rho_util_write_uint16_to_stream(buf, metadata.max_try_catch_depth);
	for (size_t i = 0; i < sizeof(buf); i++) {
		fputc(buf[i], out);
	}

	/*
	 * And now we write the actual bytecode:
	 */
	fwrite(compiler->code.bc, 1, compiler->code.size, out);

	compiler_free(compiler, true);
}
