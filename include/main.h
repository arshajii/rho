#ifndef MAIN_H
#define MAIN_H


#if !__STDC__
  #error Must be compiled with a standard-conforming compiler.
#elif __cplusplus
  #error Must not be compiled with a C++ compiler.
#elif __STDC_VERSION__ < 199901L
  #error Must be compiled with C99 or above.
#endif


/* debugging: */
#define DO_DEBUG 0

#if DO_DEBUG
  #define DEBUG() printf("%s:%d\n", __FILE__, __LINE__)
#else
  #define DEBUG() /* */
#endif


#endif /* MAIN_H */
