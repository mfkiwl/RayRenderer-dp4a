/* libjpeg-turbo build number */
#define BUILD  "20210811"

/* Compiler's inline keyword */
#undef inline

/* Define to the full name of this package. */
#define PACKAGE_NAME  "libjpeg-turbo"

/* Version number of package */
#define VERSION  "2.1.1"

#if defined(_MSC_VER)
/* How to obtain function inlining. */
#   define INLINE  __forceinline
/* How to obtain thread-local storage */
#   define THREAD_LOCAL  __declspec(thread)
/* Define if your compiler has __builtin_ctzl() and sizeof(unsigned long) == sizeof(size_t). */
/* #undef HAVE_BUILTIN_CTZL */
/* Define to 1 if you have the <intrin.h> header file. */
#   define HAVE_INTRIN_H
#else
/* How to obtain function inlining. */
#   define INLINE  __inline__ __attribute__((always_inline))
/* How to obtain thread-local storage */
#   define THREAD_LOCAL  __thread
/* Define if your compiler has __builtin_ctzl() and sizeof(unsigned long) == sizeof(size_t). */
#   define HAVE_BUILTIN_CTZL
/* Define to 1 if you have the <intrin.h> header file. */
/* #undef HAVE_INTRIN_H */
#endif

/* The size of `size_t', as computed by sizeof. */
#include <stdint.h>
#if SIZE_MAX == UINT64_MAX
#   define SIZEOF_SIZE_T  8
#else
#   define SIZEOF_SIZE_T  4
#endif

#if defined(_MSC_VER) && defined(HAVE_INTRIN_H)
#if (SIZEOF_SIZE_T == 8)
#define HAVE_BITSCANFORWARD64
#elif (SIZEOF_SIZE_T == 4)
#define HAVE_BITSCANFORWARD
#endif
#endif

#if defined(__has_attribute)
#if __has_attribute(fallthrough)
#define FALLTHROUGH  __attribute__((fallthrough));
#else
#define FALLTHROUGH
#endif
#else
#define FALLTHROUGH
#endif
