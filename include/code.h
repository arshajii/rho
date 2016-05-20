#ifndef RHO_CODE_H
#define RHO_CODE_H

#include <stdint.h>
#include "object.h"
#include "str.h"

/* fundamental unit of compiled code: 8-bit byte */
typedef uint8_t byte;

#define RHO_INT_SIZE    4
#define RHO_DOUBLE_SIZE 8

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
} RhoCode;

void rho_code_init(RhoCode *code, size_t capacity);

void rho_code_dealloc(RhoCode *code);

void rho_code_ensure_capacity(RhoCode *code, size_t min_capacity);

void rho_code_write_byte(RhoCode *code, byte b);

void rho_code_write_int(RhoCode *code, const int n);

void rho_code_write_uint16(RhoCode *code, const size_t n);

void rho_code_write_uint16_at(RhoCode *code, const size_t n, const size_t pos);

void rho_code_write_double(RhoCode *code, const double d);

void rho_code_write_str(RhoCode *code, const RhoStr *str);

void rho_code_append(RhoCode *code, const RhoCode *append);

byte rho_code_read_byte(RhoCode *code);

int rho_code_read_int(RhoCode *code);

unsigned int rho_code_read_uint16(RhoCode *code);

double rho_code_read_double(RhoCode *code);

const char *rho_code_read_str(RhoCode *code);

void rho_code_skip_ahead(RhoCode *code, const size_t skip);

void rho_code_cpy(RhoCode *dst, RhoCode *src);

#endif /* RHO_CODE_H */
