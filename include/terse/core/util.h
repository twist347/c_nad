#pragma once

/// @file

/// @defgroup core_util core/util
/// @ingroup core
/// @brief the few macros that exist because a function cannot do the job
///
/// A macro earns a place here only when a function cannot take its place: working on the
/// type itself through typeof, keeping lvalue semantics, or reaching the token rather
/// than the value.
///
/// @par Example
/// @snippet core/example_util.c swap
/// @snippet core/example_util.c stringify
/// @snippet core/example_util.c unused
/// @{

/// @name macro
/// @{

/// the inner step of TRS_STRINGIFY, and what makes the argument arrive expanded
/// @param x the tokens to spell
#define TRS_STRINGIFY_(x)   #x

/// the argument as a string literal, expanded first: TRS_STRINGIFY(TRS_STATUS_OK) is
/// "TRS_STATUS_OK", not "st"
/// @param x the tokens to spell
#define TRS_STRINGIFY(x)    TRS_STRINGIFY_(x)

/// evaluates 'val' and throws the result away, to say on purpose that a parameter or a
/// result is unused
/// @param val what to evaluate and discard
#define TRS_UNUSED(val)    ((void) (val))

/// exchanges two lvalues of one type
/// @param a one lvalue; both operands are typed from this one, so a mismatch is a
///          compile error rather than a silent truncation
/// @param b the other lvalue
/// @warning it cannot tell TRS_SWAP(self, other) from TRS_SWAP(*self, *other) — both
///          typecheck, and the first one exchanges the local pointers and leaves the
///          objects alone
#define TRS_SWAP(a, b)                                        \
    do {                                                      \
        typeof_unqual(a) *trs_swap_pa_ = &(a);                \
        typeof_unqual(a) *trs_swap_pb_ = &(b);                \
        const typeof_unqual(a) trs_swap_tmp_ = *trs_swap_pa_; \
        *trs_swap_pa_ = *trs_swap_pb_;                        \
        *trs_swap_pb_ = trs_swap_tmp_;                        \
    } while (0)

/// @}

/// @}
