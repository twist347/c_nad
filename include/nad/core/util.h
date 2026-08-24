#pragma once

/// @file

/// @defgroup core_util core/util
/// @ingroup core
/// @brief the few macros that exist because a function cannot do the job
///
/// A macro earns a place here only when a function principally cannot take its place:
/// working on the type itself through typeof, keeping lvalue semantics, or reaching the
/// token rather than the value. Everything a function can do stays a function.
///
/// @par Example
/// @snippet core/example_util.c swap
/// @snippet core/example_util.c stringify
/// @snippet core/example_util.c unused
/// @{

/// @name macro
/// @{

/// the inner step of NAD_STRINGIFY, and what makes the argument arrive expanded
/// @param x the tokens to spell
#define NAD_STRINGIFY_(x)   #x

/// the argument as a string literal, expanded first: NAD_STRINGIFY(NAD_STATUS_OK) is
/// "NAD_STATUS_OK", not "st"
/// @param x the tokens to spell
#define NAD_STRINGIFY(x)    NAD_STRINGIFY_(x)

/// evaluates 'val' and throws the result away, to say on purpose that a parameter or a
/// result is unused
/// @param val what to evaluate and discard
#define NAD_UNUSED(val)    ((void) (val))

/// exchanges two lvalues of one type
/// @param a one lvalue; both operands are typed from this one, so a mismatch is a
///          compile error rather than a silent truncation
/// @param b the other lvalue
/// @warning it cannot tell NAD_SWAP(self, other) from NAD_SWAP(*self, *other) — both
///          typecheck, and the first one exchanges the local pointers and leaves the
///          objects alone
#define NAD_SWAP(a, b)                                        \
    do {                                                      \
        typeof_unqual(a) *nad_swap_pa_ = &(a);                \
        typeof_unqual(a) *nad_swap_pb_ = &(b);                \
        const typeof_unqual(a) nad_swap_tmp_ = *nad_swap_pa_; \
        *nad_swap_pa_ = *nad_swap_pb_;                        \
        *nad_swap_pb_ = nad_swap_tmp_;                        \
    } while (0)

/// @}

/// @}
