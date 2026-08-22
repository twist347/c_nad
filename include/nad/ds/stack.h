#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// owning last in first out stack: elems are pushed and popped at one end, the top, both
/// in O(1) amortized. A ds/vec holds them — a stack touches one end only, so the ring of
/// a deque would cost it a head to keep and a wrap to check on every access for the
/// push_front it is never going to make.
///
/// What this adds over the vec it wraps is a NARROWER interface, not an invariant over
/// the elems: no get by index, no insert, no remove, no reaching the bottom. Holding a
/// nad_Stack is a statement about how the data is used, and the compiler keeps it.
///
/// The order of the elems IS the stack's meaning — it is the order they arrived in. So
/// the bridge to algo runs one way only: nad_stack_to_span is a view to read (search it,
/// count it, fold it) and there is no mutable one to sort through. Sorting a stack would
/// not be an operation on a stack.
///
/// The pointers are not stable: growing moves every elem, as in the vec and the deque.
/// That is what ds/list is for.
typedef struct nad_Stack nad_Stack;

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Status nad_stack_new(size_t elem_size, nad_Al *al, nad_Stack **out);

[[nodiscard]] NAD_API
nad_Status nad_stack_new_cap(size_t cap, size_t elem_size, nad_Al *al, nad_Stack **out);

/// the elems are pushed in the order they are given, so 'data[0]' ends up at the bottom
/// and the LAST one is on top
[[nodiscard]] NAD_API
nad_Status nad_stack_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Stack **out);

[[nodiscard]] NAD_API
nad_Status nad_stack_from_span(nad_Span s, nad_Al *al, nad_Stack **out);

NAD_API
void nad_stack_drop(nad_Stack *self);

/* ========== copy ========== */

[[nodiscard]] NAD_API
nad_Status nad_stack_copy(const nad_Stack *self, nad_Stack **out);

[[nodiscard]] NAD_API
nad_Status nad_stack_copy_assign(const nad_Stack *self, nad_Stack *other);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_stack_len(const nad_Stack *self);

[[nodiscard]] NAD_API
size_t nad_stack_cap(const nad_Stack *self);

[[nodiscard]] NAD_API
size_t nad_stack_elem_size(const nad_Stack *self);

[[nodiscard]] NAD_API
nad_Al *nad_stack_al(const nad_Stack *self);

/* ========== access ========== */

/// the elem that will be popped next; the stack must not be empty
[[nodiscard]] NAD_API
const void *nad_stack_top(const nad_Stack *self);

/// unlike a pqueue there is no order over the elems to break, so a writable top is safe:
/// what makes a stack a stack is where elems enter and leave, not what they hold
[[nodiscard]] NAD_API
void *nad_stack_top_mut(nad_Stack *self);

/* ========== mods ========== */

[[nodiscard]] NAD_API
nad_Status nad_stack_push(nad_Stack *self, const void *val);

/// drops the top elem; the stack must not be empty. Read it with top first — a pop that
/// returned the elem would have to copy it somewhere, and there is nowhere to put it
NAD_API
void nad_stack_pop(nad_Stack *self);

NAD_API
void nad_stack_clear(nad_Stack *self);

[[nodiscard]] NAD_API
nad_Status nad_stack_reserve(nad_Stack *self, size_t new_cap);

[[nodiscard]] NAD_API
nad_Status nad_stack_shrink_to_fit(nad_Stack *self);

/// on one allocator the buffers are handed over and the capacity travels with them;
/// on two the bytes are moved and each side is left sized to its new content
[[nodiscard]] NAD_API
nad_Status nad_stack_swap(nad_Stack *self, nad_Stack *other);

/* ========== to span ========== */

/// the elems bottom to top, so the top is the LAST of them and an empty stack is an
/// empty view. Read only, because the arrival order is the stack's to keep — this is the
/// whole bridge to algo. The view borrows the buffer, so the next push may invalidate it
[[nodiscard]] NAD_API
nad_Span nad_stack_to_span(const nad_Stack *self);

/* ========== print ========== */

NAD_API
void nad_stack_fprint(const nad_Stack *self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_stack_print(const nad_Stack *self, nad_FPrint fprint);

/* ========== macros ========== */

#define NAD_STACK_NEW(T, al, out) \
    nad_stack_new(sizeof(T), (al), (out))

#define NAD_STACK_NEW_CAP(T, cap, al, out) \
    nad_stack_new_cap((cap), sizeof(T), (al), (out))

#define NAD_STACK_FROM_DATA(T, data, len, al, out) \
    nad_stack_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

#define NAD_STACK_OF(T, al, out, ...)                   \
    nad_stack_from_data(                                \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

#define NAD_STACK_TOP_AS(T, self) \
    ((const T *) nad_stack_top((self)))

#define NAD_STACK_TOP_MUT_AS(T, self) \
    ((T *) nad_stack_top_mut((self)))

#define NAD_STACK_PUSH(T, self, val) \
    nad_stack_push((self), &(T){ (val) })
