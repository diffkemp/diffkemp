//===------ diffkemp_patterns.h - interface for defining C patterns -------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Kucma, tomaskucma2@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains macros and functions used to define custom C patterns.
///
//===----------------------------------------------------------------------===//

/**
 * Usage:
 *
 * This header must be included and DIFFKEMP_CPATTERN macro must be defined when
 * defining patterns. Both of these are done automatically by the pattern
 * compiler.
 *
 * To define a standard instruction pattern, first define PATTERN_NAME macro to
 * the name of the pattern and PATTERN_ARGS to the list of arguments (without
 * brackets). Then use PATTERN_OLD and PATTERN_NEW macros to define the old and
 * new variants of the pattern. To define mapping between the old and the new
 * output variables, use MAPPING macro. Variables are mapped in the order they
 * are passed to the MAPPING macro.
 *
 * Example:
 *
 *  #define PATTERN_NAME sub
 *  #define PATTERN_ARGS int x, int y, int z
 *  PATTERN_OLD {
 *      int f = x - y;
 *      MAPPING(f);
 *  }
 *  PATTERN_NEW {
 *      int f = x - z;
 *      MAPPING(f);
 *  }
 *
 * For more examples, see the tests/regression/custom_patterns/c/ folder.
 *
 * Called function can be defined in a standard way. However, if the old and new
 * functions have identical names, but different signatures, use FUNCTION_OLD
 * and FUNCTION_NEW macros to declare and call them, to avoid name colisions.
 * First macro argument is the function name, rest of the macro arguments are
 * the function arguments.
 *
 * Example of a function declaration:
 *  void FUNCTION_OLD(sub, int x, int y, int z);
 *
 * If used for a definition of a function with void return type, it can be also
 * be used to define patterns, which is specifically useful if one wants to use
 * differently named arguments in each version of the pattern. However, it is
 * still necessary that the signatures match.
 *
 * To define a pattern that ends with a resolution of a condition, use
 * CONDITION_PATTERN_OLD and CONDITION_PATTERN_NEW macros. The pattern should
 * return a boolean value, used as the condition. It is not necessary to declare
 * output mapping for the condition variable. See `condition_only.c` pattern in
 * the aforementioned example folder.
 *
 * To define a value pattern, defining a semantic equivalence between two
 * values, use VALUE_PATTERN macro. First macro argument is the function
 * name, the second and the third are the old and the new value, respectively.
 * When using extern global variables, use pointer to the value instead.
 *
 * Examples of a value pattern:
 *  VALUE_PATTERN(value, 0b110UL << 8, 0b101UL << 7);
 *  VALUE_PATTERN(global_value, 30, &extern_var);
 *
 * Patterns defined in this way can then be used by the diffkemp tool using
 * standard -p flag, in the same way as the LLVM patterns. The compiled .ll
 * pattern file will be located in the same location as the .c pattern file from
 * which it was compiled. It is also possible to purely compile the .c pattern
 * file to .ll file without performing comparison, by using the compile-pattern
 * sub-command.
 *
 * When writting patterns for kernel, it is also necessary to provide following
 * definitions and includes at the very beginning of the file, before including
 * other kernel headers:
 *  #define __KERNEL__
 *  #define __BPF_TRACING__
 *  #define __HAVE_BUILTIN_BSWAP16__
 *  #define __HAVE_BUILTIN_BSWAP32__
 *  #define __HAVE_BUILTIN_BSWAP64__
 *  #include <linux/kconfig.h>
 * Then, following include paths in the following order must be provided to the
 * compiler:
 *  -I{linux}/arch/x86/include/
 *  -I{linux}/arch/x86/include/generated/
 *  -I{linux}/include/
 *  -I{linux}/arch/x86/include/uapi
 *  -I{linux}/arch/x86/include/generated/uapi
 *  -I{linux}/include/uapi
 *  -I{linux}/include/generated/uapi
 * This can be done automatically by the pattern compiler by providing the path
 * to the kernel source files using --c-pattern-kernel-path option.
 *
 * Patterns written in C can also be loaded from YAML file, in the same way as
 * the LLVM patterns. The YAML file must contain field `patterns` with the list
 * of pattern files. Additionally, it is possible to provide extra clang options
 * for each individual pattern, using field `clang_append`, by providing a map
 * of pattern names to lists of clang options to append to them. For examples,
 * see the tests/regression/custom_patterns/c/
 */

#ifndef DIFFKEMP_SIMPLL_DIFFKEMP_PATTERNS_H
#define DIFFKEMP_SIMPLL_DIFFKEMP_PATTERNS_H

// Internal definitions

#define __DIFFKEMP_STRINGIFY_IMPL(macro) #macro
#define __DIFFKEMP_STRINGIFY(macro) __DIFFKEMP_STRINGIFY_IMPL(macro)
#define __DIFFKEMP_CONCAT_IMPL(arg1, arg2) arg1##arg2
#define __DIFFKEMP_CONCAT(arg1, arg2) __DIFFKEMP_CONCAT_IMPL(arg1, arg2)

#define __DIFFKEMP_PREFIX_OLD __diffkemp_old_
#define __DIFFKEMP_PREFIX_NEW __diffkemp_new_
#define __DIFFKEMP_MAPPING_NAME __diffkemp_output_mapping
#define __DIFFKEMP_CPATTERN_INDICATOR_NAME __diffkemp_is_cpattern

#define __DIFFKEMP_FUNCTION(version, name, ...)                                \
    __DIFFKEMP_CONCAT(__DIFFKEMP_PREFIX_##version, name)(__VA_ARGS__)

#ifdef DIFFKEMP_CPATTERN
int __DIFFKEMP_CPATTERN_INDICATOR_NAME = 1;
#if __STDC_VERSION__ >= 202000L
// In the C2x standard, functions declared with empty argument list no longer
// take any arguments, however, it is possible to use variadic arguments without
// type instead.
void __DIFFKEMP_MAPPING_NAME(...);
#else
void __DIFFKEMP_MAPPING_NAME();
#endif // __STDC_VERSION >= 202000L
#endif // DIFFKEMP_CPATTERN

// Public interface for handling patterns

/// Stringified name of a global variable, the presence of which is used to
/// detect whether a given .ll module is unpreprocessed custom C pattern.
#define CPATTERN_INDICATOR                                                     \
    __DIFFKEMP_STRINGIFY(__DIFFKEMP_CPATTERN_INDICATOR_NAME)

/// String versions of naming schemes.
#define CPATTERN_OLD_PREFIX __DIFFKEMP_STRINGIFY(__DIFFKEMP_PREFIX_OLD)
#define CPATTERN_NEW_PREFIX __DIFFKEMP_STRINGIFY(__DIFFKEMP_PREFIX_NEW)
#define CPATTERN_OUTPUT_MAPPING_NAME                                           \
    __DIFFKEMP_STRINGIFY(__DIFFKEMP_MAPPING_NAME)

// Public interface for defining patterns

#ifdef DIFFKEMP_CPATTERN

/// Used to define old/new variants of a function with identical name, but
/// different signature, to avoid conflicting definitions. Can be also used to
/// define patterns, if provided definition, for example to use different names
/// for arguments.
#define FUNCTION_OLD(name, ...) __DIFFKEMP_FUNCTION(OLD, name, __VA_ARGS__)
#define FUNCTION_NEW(name, ...) __DIFFKEMP_FUNCTION(NEW, name, __VA_ARGS__)

/// Used to define standard instruction patterns. To use, first define
/// PATTERN_NAME macro to the name of the pattern and PATTERN_ARGS to the list
/// of arguments (without parentheses).
#define PATTERN_OLD void FUNCTION_OLD(PATTERN_NAME, PATTERN_ARGS)
#define PATTERN_NEW void FUNCTION_NEW(PATTERN_NAME, PATTERN_ARGS)

/// Used to define condition pattern. Use identically as standard pattern, only
/// the pattern should return a boolean value, used as the condition.
#define CONDITION_PATTERN_OLD _Bool FUNCTION_OLD(PATTERN_NAME, PATTERN_ARGS)
#define CONDITION_PATTERN_NEW _Bool FUNCTION_NEW(PATTERN_NAME, PATTERN_ARGS)

/// Used to define value patterns. To use, simply provide the old and the new
/// value.
#define VALUE_PATTERN(name, old_value, new_value)                              \
    __typeof__(old_value) FUNCTION_OLD(name, ) { return old_value; }           \
    __typeof__(new_value) FUNCTION_NEW(name, ) { return new_value; }

#define MAPPING(...) __DIFFKEMP_MAPPING_NAME(__VA_ARGS__)

#endif // DIFFKEMP_CPATTERN

#endif // DIFFKEMP_SIMPLL_DIFFKEMP_PATTERNS_H
