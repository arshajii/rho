#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

#define getmember(instance, offset, type) (*(type *)((char *)instance + offset))

#define UNUSED(x) (void)(x)

#define DECL_MIN_FUNC(name, type) \
	static inline type name(const type a, const type b) { return a < b ? a : b; }

#define DECL_MAX_FUNC(name, type) \
	static inline type name(const type a, const type b) { return a > b ? a : b; }

int hash_int(const int i);
int hash_double(const double d);
int hash_float(const float f);
int hash_bool(const bool b);
int hash_ptr(const void *p);
int secondary_hash(int hash);

void write_int32_to_stream(unsigned char *stream, const int n);
int read_int32_from_stream(unsigned char *stream);
void write_uint16_to_stream(unsigned char *stream, const unsigned int n);
unsigned int read_uint16_from_stream(unsigned char *stream);
void write_double_to_stream(unsigned char *stream, const double d);
double read_double_from_stream(unsigned char *stream);

void *safe_malloc(const size_t size);

struct str_array {
	/* bare-bones string array */
	struct {
		const char *str;
		size_t length;
	} *array;

	size_t length;
};

#endif /* UTIL_H */
