#ifndef RHO_UTIL_H
#define RHO_UTIL_H

#include <stddef.h>
#include <stdbool.h>

#define RHO_ANSI_CLR_RED     "\x1b[31m"
#define RHO_ANSI_CLR_GREEN   "\x1b[32m"
#define RHO_ANSI_CLR_YELLOW  "\x1b[33m"
#define RHO_ANSI_CLR_BLUE    "\x1b[34m"
#define RHO_ANSI_CLR_MAGENTA "\x1b[35m"
#define RHO_ANSI_CLR_CYAN    "\x1b[36m"
#define RHO_ANSI_CLR_INFO    "\x1b[1;36m"
#define RHO_ANSI_CLR_WARNING "\x1b[1;33m"
#define RHO_ANSI_CLR_ERROR   "\x1b[1;31m"
#define RHO_ANSI_CLR_RESET   "\x1b[0m"

#define RHO_INFO_HEADER    RHO_ANSI_CLR_INFO    "info:    " RHO_ANSI_CLR_RESET
#define RHO_WARNING_HEADER RHO_ANSI_CLR_WARNING "warning: " RHO_ANSI_CLR_RESET
#define RHO_ERROR_HEADER   RHO_ANSI_CLR_ERROR   "error:   " RHO_ANSI_CLR_RESET

#define rho_getmember(instance, offset, type) (*(type *)((char *)instance + offset))

#define RHO_UNUSED(x) (void)(x)

int rho_util_hash_int(const int i);
int rho_util_hash_long(const long l);
int rho_util_hash_double(const double d);
int rho_util_hash_float(const float f);
int rho_util_hash_bool(const bool b);
int rho_util_hash_ptr(const void *p);
int rho_util_hash_cstr(const char *str);
int rho_util_hash_cstr2(const char *str, const size_t len);
int rho_util_hash_secondary(int hash);

void rho_util_write_int32_to_stream(unsigned char *stream, const int n);
int rho_util_read_int32_from_stream(unsigned char *stream);
void rho_util_write_uint16_to_stream(unsigned char *stream, const unsigned int n);
unsigned int rho_util_read_uint16_from_stream(unsigned char *stream);
void rho_util_write_double_to_stream(unsigned char *stream, const double d);
double rho_util_read_double_from_stream(unsigned char *stream);

void *rho_malloc(size_t n);
void *rho_calloc(size_t num, size_t size);
void *rho_realloc(void *p, size_t n);
#define RHO_FREE(ptr) free((void *)(ptr))

const char *rho_util_str_dup(const char *str);
const char *rho_util_str_format(const char *format, ...);

char *rho_util_file_to_str(const char *filename);

size_t rho_smallest_pow_2_at_least(size_t x);

#endif /* RHO_UTIL_H */
