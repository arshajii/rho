#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err.h"
#include "util.h"

/*
 * Hash functions
 */

int hash_int(const int i)
{
	return i;
}

int hash_long(const long l)
{
	return (int)(l ^ ((unsigned long)l >> 32));
}

int hash_double(const double d)
{
	unsigned long long l = 0;
	memcpy(&l, &d, sizeof(double));
	return (int)(l ^ (l >> 32));
}

int hash_float(const float f)
{
	unsigned long long l = 0;
	memcpy(&l, &f, sizeof(float));
	return (int)(l ^ (l >> 32));
}

int hash_bool(const bool b)
{
	return b ? 1231 : 1237;
}

int hash_ptr(const void *p)
{
	uintptr_t ad = (uintptr_t)p;
	return (int)((13*ad) ^ (ad >> 15));
}

int hash_cstr(const char *str)
{
	unsigned int h = 0;
	char *p = (char *)str;

	while (*p++) {
		h = 31*h + *p;
	}

	return h;
}

int hash_cstr2(const char *str, const size_t len)
{
	unsigned int h = 0;

	for (size_t i = 0; i < len; i++) {
		h = 31*h + str[i];
	}

	return h;
}

/* Adapted from java.util.HashMap#hash */
int secondary_hash(int h)
{
	h ^= ((unsigned)h >> 20) ^ ((unsigned)h >> 12);
	return h ^ ((unsigned)h >> 7) ^ ((unsigned)h >> 4);
}

/*
 * Serialization functions
 */

void write_int32_to_stream(unsigned char *stream, const int n)
{
	stream[0] = (n >> 0 ) & 0xFF;
	stream[1] = (n >> 8 ) & 0xFF;
	stream[2] = (n >> 16) & 0xFF;
	stream[3] = (n >> 24) & 0xFF;
}

int read_int32_from_stream(unsigned char *stream)
{
	const int n = (stream[3] << 24) |
	              (stream[2] << 16) |
	              (stream[1] << 8 ) |
	              (stream[0] << 0 );
	return n;
}

void write_uint16_to_stream(unsigned char *stream, const unsigned int n)
{
	assert(n <= 0xFFFF);
	stream[0] = (n >> 0) & 0xFF;
	stream[1] = (n >> 8) & 0xFF;
}

unsigned int read_uint16_from_stream(unsigned char *stream)
{
	const unsigned int n = (stream[1] << 8) | (stream[0] << 0);
	return n;
}

/*
 * TODO: check endianness
 */
void write_double_to_stream(unsigned char *stream, const double d)
{
	memcpy(stream, &d, sizeof(double));
}

double read_double_from_stream(unsigned char *stream)
{
	double d;
	memcpy(&d, stream, sizeof(double));
	return d;
}

/*
 * Memory allocation functions
 */

void *rho_malloc(size_t n)
{
	void *p = malloc(n);

	if (p == NULL && n > 0) {
		OUT_OF_MEM_ERROR();
	}

	return p;
}

void *rho_calloc(size_t num, size_t size)
{
	void *p = calloc(num, size);

	if (p == NULL && num > 0 && size > 0) {
		OUT_OF_MEM_ERROR();
	}

	return p;
}

void *rho_realloc(void *p, size_t n)
{
	void *new_p = realloc(p, n);

	if (new_p == NULL && n > 0) {
		OUT_OF_MEM_ERROR();
	}

	return new_p;
}

/*
 * Miscellaneous functions
 */

/* http://stackoverflow.com/questions/1001307/ */
bool is_big_endian(void)
{
	union {
		uint32_t i;
		char c[4];
	} bint = {0x01020304};

	return (bint.c[0] == 1);
}
