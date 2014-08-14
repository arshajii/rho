#include <stdlib.h>
#include <string.h>
#include "util.h"

int hash_int(const int i)
{
	return i;
}

int hash_double(const double d)
{
	unsigned long long l;
	memcpy(&l, &d, sizeof(double));
	return l ^ (l >> 32);
}

int hash_float(const float f)
{
	unsigned long long l;
	memcpy(&l, &f, sizeof(float));
	return l ^ (l >> 32);
}

int hash_bool(const bool b)
{
	return b ? 1231 : 1237;
}

int hash_ptr(const void *p)
{
	uintptr_t ad = (uintptr_t) p;
	return (size_t) ((13*ad) ^ (ad >> 15));
}

/*
 * Adapted from java.util.HashMap#hash
 */
int secondary_hash(int h)
{
	h ^= ((unsigned)h >> 20) ^ ((unsigned)h >> 12);
	return h ^ ((unsigned)h >> 7) ^ ((unsigned)h >> 4);
}

void *safe_malloc(const size_t size)
{
	void *mem = malloc(size);

	if (mem == NULL) {
		abort();
	}

	return mem;
}
