#include <string.h>
#include "compiler.h"
#include "code.h"

void code_init(Code *code, size_t capacity)
{
	code->bc = malloc(capacity);
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
		code->bc = realloc(code->bc, code->capacity);
	}
}

void code_write_byte(Code *code, const byte b)
{
	code_ensure_capacity(code, code->size + 1);
	code->bc[code->size++] = b;
}

void code_write_int(Code *code, const int n)
{
	code_ensure_capacity(code, code->size + INT_SIZE);
	memcpy(code->bc + code->size, &n, INT_SIZE);
	code->size += INT_SIZE;
}

void code_write_double(Code *code, const double d)
{
	code_ensure_capacity(code, code->size + DOUBLE_SIZE);
	memcpy(code->bc + code->size, &d, DOUBLE_SIZE);
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

Code code_sub(Code *code, const unsigned int off, const size_t len)
{
	return (Code){.bc = code->bc + off, .size = len, .capacity = code->capacity - off};
}

byte code_read_byte(Code *code)
{
	--code->size;
	return *code->bc++;
}

int code_read_int(Code *code)
{
	code->size -= INT_SIZE;
	const int ret = read_int(code->bc);
	code->bc += INT_SIZE;
	return ret;
}

double code_read_double(Code *code)
{
	code->size -= DOUBLE_SIZE;
	const double ret = read_double(code->bc);
	code->bc += DOUBLE_SIZE;
	return ret;
}

const char *code_read_str(Code *code)
{
	size_t len = 0;

	while (code->bc[len] != '\0') {
		++len;
	}

	char *str = malloc(len + 1);
	for (size_t j = 0; j < len; j++) {
		str[j] = code_read_byte(code);
	}
	code_read_byte(code);  /* skip the string termination byte */
	str[len] = '\0';
	return str;
}

void code_cpy(Code *dst, Code *src)
{
	dst->bc = malloc(src->capacity);
	memcpy(dst->bc, src->bc, src->size);
	dst->size = src->size;
	dst->capacity = src->capacity;
}
