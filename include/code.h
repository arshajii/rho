#ifndef CODE_H
#define CODE_H

#include <stdint.h>
#include "object.h"
#include "str.h"
#include "util.h"

/* fundamental unit of compiled code: 8-bit byte */
typedef uint8_t byte;

#define INT_SIZE 4
#define DOUBLE_SIZE 8

/*
 * Low-level bytecode storage facility
 */
typedef struct {
	/* bytecode */
	byte *bc;

	/* current size in bytes */
	size_t size;

	/* capacity of allocated array */
	size_t capacity;
} Code;

void code_init(Code *code, size_t capacity);

void code_dealloc(Code *code);

void code_ensure_capacity(Code *code, size_t min_capacity);

void code_write_byte(Code *code, byte b);

void code_write_int(Code *code, const int n);

void code_write_uint16(Code *code, const size_t n);

void code_write_uint16_at(Code *code, const size_t n, const size_t pos);

void code_write_double(Code *code, const double d);

void code_write_str(Code *code, const Str *str);

void code_append(Code *code, const Code *append);

byte code_read_byte(Code *code);

int code_read_int(Code *code);

unsigned int code_read_uint16(Code *code);

double code_read_double(Code *code);

const char *code_read_str(Code *code);

void code_skip_ahead(Code *code, const size_t skip);

void code_cpy(Code *dst, Code *src);

#endif /* CODE_H */
