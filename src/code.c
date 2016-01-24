#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "compiler.h"
#include "err.h"
#include "util.h"
#include "code.h"

void code_init(Code *code, size_t capacity)
{
	code->bc = rho_malloc(capacity);
	code->size = 0;
	code->capacity = capacity;
}

void code_dealloc(Code *code)
{
	free(code->bc);
}

void code_ensure_capacity(Code *code, size_t min_capacity)
{
	size_t capacity = code->capacity;
	if (capacity < min_capacity) {
		while (capacity < min_capacity) {
			capacity <<= 1;
		}

		code->capacity = capacity;
		code->bc = rho_realloc(code->bc, capacity);
	}
}

void code_write_byte(Code *code, const byte b)
{
	code_ensure_capacity(code, code->size + 1);
	code->bc[code->size++] = b;
}

/*
 * This is typically used for writing "sizes" to the bytecode
 * (for example, the size of the symbol table), which is why
 * `n` is of type `size_t`. An error will be emitted if `n`
 * is too large, however.
 */
void code_write_uint16(Code *code, const size_t n)
{
	code_write_uint16_at(code, n, code->size);
}

void code_write_uint16_at(Code *code, const size_t n, const size_t pos)
{
	if (n > 0xFFFF) {
		INTERNAL_ERROR();
	}

	const size_t code_size = code->size;

	if (pos > code_size) {
		INTERNAL_ERROR();
	}

	if (code_size < 2 || pos > code_size - 2) {
		code_ensure_capacity(code, pos + 2);
		code->size += (pos + 2 - code_size);
	}

	write_uint16_to_stream(code->bc + pos, n);
}

void code_write_int(Code *code, const int n)
{
	code_ensure_capacity(code, code->size + INT_SIZE);
	write_int32_to_stream(code->bc + code->size, n);
	code->size += INT_SIZE;
}

void code_write_double(Code *code, const double d)
{
	code_ensure_capacity(code, code->size + DOUBLE_SIZE);
	write_double_to_stream(code->bc + code->size, d);
	code->size += DOUBLE_SIZE;
}

void code_write_str(Code *code, const Str *str)
{
	const size_t len = str->len;
	code_ensure_capacity(code, code->size + len + 1);

	for (size_t i = 0; i < len; i++) {
		code->bc[code->size++] = str->value[i];
	}
	code->bc[code->size++] = '\0';
}

void code_append(Code *code, const Code *append)
{
	const size_t size = append->size;
	code_ensure_capacity(code, code->size + size);

	memcpy(code->bc + code->size, append->bc, size);
	code->size += size;
}

byte code_read_byte(Code *code)
{
	--code->size;
	return *code->bc++;
}

int code_read_int(Code *code)
{
	code->size -= INT_SIZE;
	const int ret = read_int32_from_stream(code->bc);
	code->bc += INT_SIZE;
	return ret;
}

unsigned int code_read_uint16(Code *code)
{
	code->size -= 2;
	const unsigned int ret = read_uint16_from_stream(code->bc);
	code->bc += 2;
	return ret;
}

double code_read_double(Code *code)
{
	code->size -= DOUBLE_SIZE;
	const double ret = read_double_from_stream(code->bc);
	code->bc += DOUBLE_SIZE;
	return ret;
}

const char *code_read_str(Code *code)
{
	const char *start = (const char *)code->bc;

	while (*(code->bc++) != '\0');

	return start;
}

void code_skip_ahead(Code *code, const size_t skip)
{
	assert(skip <= code->size);
	code->bc += skip;
	code->size -= skip;
}

void code_cpy(Code *dst, Code *src)
{
	dst->bc = rho_malloc(src->capacity);
	memcpy(dst->bc, src->bc, src->size);
	dst->size = src->size;
	dst->capacity = src->capacity;
}
