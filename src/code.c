#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "compiler.h"
#include "err.h"
#include "util.h"
#include "code.h"

void rho_code_init(RhoCode *code, size_t capacity)
{
	code->bc = rho_malloc(capacity);
	code->size = 0;
	code->capacity = capacity;
}

void rho_code_dealloc(RhoCode *code)
{
	free(code->bc);
}

void rho_code_ensure_capacity(RhoCode *code, size_t min_capacity)
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

void rho_code_write_byte(RhoCode *code, const byte b)
{
	rho_code_ensure_capacity(code, code->size + 1);
	code->bc[code->size++] = b;
}

/*
 * This is typically used for writing "sizes" to the bytecode
 * (for example, the size of the symbol table), which is why
 * `n` is of type `size_t`. An error will be emitted if `n`
 * is too large, however.
 */
void rho_code_write_uint16(RhoCode *code, const size_t n)
{
	rho_code_write_uint16_at(code, n, code->size);
}

void rho_code_write_uint16_at(RhoCode *code, const size_t n, const size_t pos)
{
	if (n > 0xFFFF) {
		RHO_INTERNAL_ERROR();
	}

	const size_t code_size = code->size;

	if (pos > code_size) {
		RHO_INTERNAL_ERROR();
	}

	if (code_size < 2 || pos > code_size - 2) {
		rho_code_ensure_capacity(code, pos + 2);
		code->size += (pos + 2 - code_size);
	}

	rho_util_write_uint16_to_stream(code->bc + pos, n);
}

void rho_code_write_int(RhoCode *code, const int n)
{
	rho_code_ensure_capacity(code, code->size + RHO_INT_SIZE);
	rho_util_write_int32_to_stream(code->bc + code->size, n);
	code->size += RHO_INT_SIZE;
}

void rho_code_write_double(RhoCode *code, const double d)
{
	rho_code_ensure_capacity(code, code->size + RHO_DOUBLE_SIZE);
	rho_util_write_double_to_stream(code->bc + code->size, d);
	code->size += RHO_DOUBLE_SIZE;
}

void rho_code_write_str(RhoCode *code, const RhoStr *str)
{
	const size_t len = str->len;
	rho_code_ensure_capacity(code, code->size + len + 1);

	for (size_t i = 0; i < len; i++) {
		code->bc[code->size++] = str->value[i];
	}
	code->bc[code->size++] = '\0';
}

void rho_code_append(RhoCode *code, const RhoCode *append)
{
	const size_t size = append->size;
	rho_code_ensure_capacity(code, code->size + size);

	memcpy(code->bc + code->size, append->bc, size);
	code->size += size;
}

byte rho_code_read_byte(RhoCode *code)
{
	--code->size;
	return *code->bc++;
}

int rho_code_read_int(RhoCode *code)
{
	code->size -= RHO_INT_SIZE;
	const int ret = rho_util_read_int32_from_stream(code->bc);
	code->bc += RHO_INT_SIZE;
	return ret;
}

unsigned int rho_code_read_uint16(RhoCode *code)
{
	code->size -= 2;
	const unsigned int ret = rho_util_read_uint16_from_stream(code->bc);
	code->bc += 2;
	return ret;
}

double rho_code_read_double(RhoCode *code)
{
	code->size -= RHO_DOUBLE_SIZE;
	const double ret = rho_util_read_double_from_stream(code->bc);
	code->bc += RHO_DOUBLE_SIZE;
	return ret;
}

const char *rho_code_read_str(RhoCode *code)
{
	const char *start = (const char *)code->bc;

	while (*(code->bc++) != '\0');

	return start;
}

void rho_code_skip_ahead(RhoCode *code, const size_t skip)
{
	assert(skip <= code->size);
	code->bc += skip;
	code->size -= skip;
}

void rho_code_cpy(RhoCode *dst, RhoCode *src)
{
	dst->bc = rho_malloc(src->capacity);
	memcpy(dst->bc, src->bc, src->size);
	dst->size = src->size;
	dst->capacity = src->capacity;
}
