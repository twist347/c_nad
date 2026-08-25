#pragma once

#include <stddef.h>

/// @file

/// @defgroup algo_fn algo/fn
/// @ingroup algo
/// @brief the function-pointer types algo is customized through
///
/// The only knobs algo has. Each takes its elems by address and a 'ctx' the caller owns
/// and the algo never reads — parameterizing an algo without a global. Ordering and
/// equality live apart in core/cmp, which has ready-made values where these cannot.
///
/// @{

/// Whether an elem passes. Takes the elem and the caller's 'ctx'.
typedef bool (*nad_Pred)(const void *elem, void *ctx);

/// Folds one elem into the accumulator. 'acc' belongs to the caller and may be of any
/// type — that is what lets a span of int32_t fold into an int64_t.
typedef void (*nad_Fold)(void *acc, const void *elem, void *ctx);

/// Produces the elem for position 'idx', writing it to 'dst'.
typedef void (*nad_Gen)(void *dst, size_t idx, void *ctx);

/// Produces the elem written to 'dst' from the one at 'src'.
typedef void (*nad_UnOp)(void *dst, const void *src, void *ctx);

/// Combines two elems into 'dst': 'dst' is written, 'a' and 'b' are read.
typedef void (*nad_BinOp)(void *dst, const void *a, const void *b, void *ctx);

/// @}
