#pragma once

#include <stddef.h>

typedef bool (*nad_Pred)(const void *elem, void *ctx);

/// folds one elem into the accumulator. 'acc' belongs to the caller and may
/// be of any type — that is what lets a span of int32_t fold into an int64_t
typedef void (*nad_Fold)(void *acc, const void *elem, void *ctx);

/// produces the elem for position 'idx'
typedef void (*nad_Gen)(void *dst, size_t idx, void *ctx);

/// produces the elem written to 'dst' from the one at 'src'
typedef void (*nad_UnOp)(void *dst, const void *src, void *ctx);

/// combines two elems into 'dst'. 'dst' is written, 'a' and 'b' are read
typedef void (*nad_BinOp)(void *dst, const void *a, const void *b, void *ctx);

