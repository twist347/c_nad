#pragma once

#include "nad/core/export.h"
#include "nad/core/print.h"

#include <stddef.h>
#include <assert.h>

/// @file

/// @defgroup core_span core/span
/// @ingroup core
/// @brief nad_Span and nad_SpanMut — non-owning views over contiguous elems
///
/// Three fields — where the elems are, how many, how big one is — passed by value. A view
/// borrows: it never allocates, never frees, never extends the life of what it points at.
///
/// The struct is transparent, so 's.len' is a field and a compound literal is already a
/// view. There are two of them because a const struct would not make its pointee const:
/// nad_Span reads, nad_SpanMut writes, nad_span_mut_to_span converts one way only.
///
/// The seam between ds and algo. Nothing here allocates, so nothing returns a nad_Status,
/// and an index out of range asserts. A view must not outlive what it views, nor survive
/// an op that moves the source block — nothing checks that.
///
/// @par Example
/// @snippet core/example_span.c build
/// @snippet core/example_span.c access
/// @snippet core/example_span.c sub
/// @snippet core/example_span.c bridge
/// @{

/// A read-only view over contiguous elems.
typedef struct {
    const void *data;  ///< the first elem; null only while 'len' is 0
    size_t len;        ///< how many elems the view spans
    size_t elem_size;  ///< the size of one elem in bytes, greater than 0
} nad_Span;

/// A writable view over contiguous elems: nad_Span with a 'data' to write through.
typedef struct {
    void *data;        ///< the first elem; null only while 'len' is 0
    size_t len;        ///< how many elems the view spans
    size_t elem_size;  ///< the size of one elem in bytes, greater than 0
} nad_SpanMut;

/// @name invariant
/// @{

/// asserts what every view holds: 'data' set unless empty, 'elem_size' over 0
/// @param s the view, of either type
#define NAD_SPAN_ASSERT(s)             \
    (assert((s).data || (s).len == 0), \
     assert((s).elem_size > 0))

/// @}

/// @name construction
/// @{

/// a read-only view over 'len' elems at 'data'
/// @param data the first elem; null only when 'len' is 0
/// @param len elems in the view
/// @param elem_size bytes in one elem, greater than 0
/// @return the view
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Span nad_span_new(const void *data, size_t len, size_t elem_size);

/// nad_span_new, writable
/// @param data the first elem; null only when 'len' is 0
/// @param len elems in the view
/// @param elem_size bytes in one elem, greater than 0
/// @return the view
/// @bigo{1}
[[nodiscard]] NAD_API
nad_SpanMut nad_span_new_mut(void *data, size_t len, size_t elem_size);

/// @}

/// @name to span
/// @{

/// the same elems, seen read-only
/// @param s the view to give up the write rights of
/// @return the read-only view; there is no way back
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Span nad_span_mut_to_span(nad_SpanMut s);

/// @}

/// @name subspan
/// @{

/// a view over 'count' elems of 'self', starting at 'start'
/// @param self the view to narrow
/// @param start where it begins; asserts start <= self.len, == gives an empty view
/// @param count elems it spans; asserts count <= self.len - start
/// @return the narrower view, over the same memory — nothing is copied
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Span nad_span_sub(nad_Span self, size_t start, size_t count);

/// nad_span_sub, writable
/// @param self the view to narrow
/// @param start where it begins; asserts start <= self.len, == gives an empty view
/// @param count elems it spans; asserts count <= self.len - start
/// @return the narrower view, over the same memory — nothing is copied
/// @bigo{1}
[[nodiscard]] NAD_API
nad_SpanMut nad_span_sub_mut(nad_SpanMut self, size_t start, size_t count);

/// @}

/// @name info
/// @{

/// the size of what the view spans; a writable view converts first
/// @param self the view
/// @return len * elem_size
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_span_bytes(nad_Span self);

/// @}

/// @name access
/// @{

/// the elem at 'idx'
/// @param self the view
/// @param idx the index; asserts idx < self.len
/// @return a pointer to the elem
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_span_get(nad_Span self, size_t idx);

/// nad_span_get, to write through
/// @param self the view
/// @param idx the index; asserts idx < self.len
/// @return a pointer to the elem
/// @bigo{1}
[[nodiscard]] NAD_API
void *nad_span_get_mut(nad_SpanMut self, size_t idx);

/// writes one elem over the elem at 'idx', in the borrowed memory itself
/// @param self the view
/// @param idx the index; asserts idx < self.len
/// @param val the address of the value; elem_size bytes are read from it
/// @bigo{1}
NAD_API
void nad_span_set(nad_SpanMut self, size_t idx, const void *val);

/// @}

/// @name mods
/// @{

/// exchanges two elems in place
/// @param self the view
/// @param i one index; asserts it is in range
/// @param j the other index; asserts it is in range; i == j is a no-op
/// @bigo{1}
NAD_API
void nad_span_swap_elems(nad_SpanMut self, size_t i, size_t j);

/// @}

/// @name print
/// @{

/// writes the elems to a stream as [a, b, c], followed by a newline
/// @param self the view
/// @param stream where to write
/// @param fprint the printer, one call per elem
/// @bigo{n}
NAD_API
void nad_span_fprint(nad_Span self, FILE *stream, nad_FPrint fprint);

/// nad_span_fprint, over a writable view
/// @param self the view
/// @param stream where to write
/// @param fprint the printer, one call per elem
/// @bigo{n}
NAD_API
void nad_span_mut_fprint(nad_SpanMut self, FILE *stream, nad_FPrint fprint);

/// nad_span_fprint to stdout: [a, b, c] and a newline
/// @param self the view
/// @param fprint the printer, one call per elem
/// @bigo{n}
NAD_API
void nad_span_print(nad_Span self, nad_FPrint fprint);

/// nad_span_print, over a writable view
/// @param self the view
/// @param fprint the printer, one call per elem
/// @bigo{n}
NAD_API
void nad_span_mut_print(nad_SpanMut self, nad_FPrint fprint);

/// @}

/// @name macros
/// @{

/// nad_span_new with sizeof(T) for the elem size
/// @param T the elem type
/// @param data the first elem, made to typecheck as a const T *
/// @param len elems in the view
/// @bigo{1}
#define NAD_SPAN_NEW(T, data, len) \
    nad_span_new((const T *){ (data) }, (len), sizeof(T))

/// nad_span_new_mut with sizeof(T)
/// @param T the elem type
/// @param data the first elem, made to typecheck as a T *
/// @param len elems in the view
/// @bigo{1}
#define NAD_SPAN_NEW_MUT(T, data, len) \
    nad_span_new_mut((T *){ (data) }, (len), sizeof(T))

/// a view over the elems written out: NAD_SPAN_OF(int32_t, 5, 3, 1)
/// @param T the elem type
/// @param ... the elems, as a T initializer list
/// @warning the elems are a compound literal, gone at the end of the enclosing block
/// @bigo{1}
#define NAD_SPAN_OF(T, ...)                             \
    nad_span_new(                                       \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T))

/// NAD_SPAN_OF, writable
/// @param T the elem type
/// @param ... the elems, as a T initializer list
/// @warning the elems are a compound literal, gone at the end of the enclosing block
/// @bigo{1}
#define NAD_SPAN_OF_MUT(T, ...)                   \
    nad_span_new_mut(                             \
        (T[]){ __VA_ARGS__ },                     \
        sizeof((T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T))

/// nad_span_get as a const T *
/// @param T the elem type
/// @param s the view
/// @param idx the index
/// @bigo{1}
#define NAD_SPAN_GET_AS(T, s, idx) \
    ((const T *) nad_span_get((s), (idx)))

/// nad_span_get_mut as a T *
/// @param T the elem type
/// @param s the view
/// @param idx the index
/// @bigo{1}
#define NAD_SPAN_GET_MUT_AS(T, s, idx) \
    ((T *) nad_span_get_mut((s), (idx)))

/// nad_span_set from a value rather than its address
/// @param T the elem type — scalars only, (T){ val } takes nothing else
/// @param s the view
/// @param idx the index
/// @param val the value to copy in
/// @bigo{1}
#define NAD_SPAN_SET(T, s, idx, val) \
    nad_span_set((s), (idx), &(T){ (val) })

/// @}

/// @}
