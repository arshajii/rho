#ifndef RHO_MAIN_H
#define RHO_MAIN_H

#define RHO_VERSION "0.0.0"

#define RHO_IS_POSIX (defined (__unix__) || (defined (__APPLE__) && defined (__MACH__)))

#define RHO_THREADED (RHO_IS_POSIX && __STDC_VERSION__ >= 201112L && !defined (__STDC_NO_ATOMICS__))

#endif /* RHO_MAIN_H */
