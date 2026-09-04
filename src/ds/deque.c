#include "nad/ds/deque.h"

#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

/* ========== internals ========== */

#define ASSERT_DEQUE(d)                             \
    (assert(d),                                     \
     assert((d)->elem_size > 0),                    \
     assert((d)->len <= (d)->cap),                  \
     assert((d)->cap == 0 || (d)->data),            \
     assert(!(d)->data || (d)->cap > 0),            \
     assert((d)->cap == 0 || (d)->head < (d)->cap), \
     assert((d)->cap > 0 || (d)->head == 0),        \
     assert((d)->al))

static constexpr size_t DEQUE_GROWTH_BASE = 1;
static constexpr size_t DEQUE_GROWTH_FACTOR = 2;

struct nad_Deque {
    void *data;
    size_t len;
    size_t cap;
    // slot holding the front elem. Kept alongside 'len' rather than a tail index:
    // with two indices a full ring and an empty one look exactly alike
    size_t head;
    size_t elem_size;
    nad_Al *al;
};

[[nodiscard]]
static nad_Status new_impl(bool zeroed, size_t len, size_t cap, size_t elem_size, nad_Al *al, nad_Deque **out);

static void set_fields(nad_Deque *obj, void *data, size_t len, size_t cap, size_t elem_size, nad_Al *al);

[[nodiscard]]
static size_t next_cap(const nad_Deque *self);

[[nodiscard]]
static nad_Status grow(nad_Deque *self);

[[nodiscard]]
static size_t len_bytes(const nad_Deque *self);

[[nodiscard]]
static size_t cap_bytes(const nad_Deque *self);

/// the slot holding the elem at 'idx' counted from the front. One subtraction and no
/// division: 'idx' is below cap, so head + idx overshoots by less than one lap
[[nodiscard]]
static size_t slot_of(const nad_Deque *self, size_t idx);

[[nodiscard]]
static const unsigned char *slot_at(const nad_Deque *self, size_t slot);

[[nodiscard]]
static unsigned char *slot_at_mut(nad_Deque *self, size_t slot);

[[nodiscard]]
static const unsigned char *elem_at(const nad_Deque *self, size_t idx);

[[nodiscard]]
static unsigned char *elem_at_mut(nad_Deque *self, size_t idx);

/// how many elems sit between head and the end of the buffer — the length of the first
/// of the at most two runs the contents form
[[nodiscard]]
static size_t first_run(const nad_Deque *self);

/// every elem, in ring order, into 'dst' bytes. The caller guarantees the room
static void copy_out(const nad_Deque *self, void *dst);

/// the inverse: 'src' bytes over every elem, in ring order
static void copy_in(nad_Deque *self, const void *src);

static void move_elem(nad_Deque *self, size_t to, size_t from);

/* ========== lifetime ========== */

nad_Status nad_deque_new(size_t elem_size, nad_Al *al, nad_Deque **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(false, 0, 0, elem_size, al, out);
}

nad_Status nad_deque_new_len(size_t len, size_t elem_size, nad_Al *al, nad_Deque **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(true, len, len, elem_size, al, out);
}

nad_Status nad_deque_new_cap(size_t cap, size_t elem_size, nad_Al *al, nad_Deque **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(false, 0, cap, elem_size, al, out);
}

nad_Status nad_deque_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Deque **out) {
    assert(data || len == 0);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Deque *deque;
    const nad_Status st = new_impl(false, len, len, elem_size, al, &deque);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    // head is 0 here, so the ring starts out unwrapped and this is one memcpy
    if (len > 0) {
        memcpy(deque->data, data, len_bytes(deque));
    }

    *out = deque;

    return NAD_STATUS_OK;
}

nad_Status nad_deque_from_span(nad_Span s, nad_Al *al, nad_Deque **out) {
    NAD_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return nad_deque_from_data(s.data, s.len, s.elem_size, al, out);
}

void nad_deque_drop(nad_Deque *self) {
    if (!self) {
        return;
    }

    ASSERT_DEQUE(self);

    nad_Al *al_copy = self->al;
    nad_dealloc(al_copy, self->data, cap_bytes(self));
    nad_dealloc(al_copy, self, sizeof(nad_Deque));
}

/* ========== copy ========== */

nad_Status nad_deque_copy(const nad_Deque *self, nad_Deque **out) {
    ASSERT_DEQUE(self);

    return nad_deque_copy_with(self, self->al, out);
}

nad_Status nad_deque_copy_with(const nad_Deque *self, nad_Al *al, nad_Deque **out) {
    ASSERT_DEQUE(self);
    assert(al);
    assert(out);

    nad_Deque *copy;
    const nad_Status st = new_impl(false, self->len, self->len, self->elem_size, al, &copy);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    // the copy is sized to the content, so it comes out unwrapped whatever self looks like
    copy_out(self, copy->data);

    *out = copy;

    return NAD_STATUS_OK;
}

nad_Status nad_deque_copy_assign(const nad_Deque *self, nad_Deque *other) {
    ASSERT_DEQUE(self);
    ASSERT_DEQUE(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    const nad_Status st = nad_deque_reserve(other, self->len);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    // whatever 'other' held is gone, so its ring is laid out afresh from slot 0
    other->head = 0;
    other->len = self->len;
    copy_out(self, other->data);

    ASSERT_DEQUE(other);

    return NAD_STATUS_OK;
}

void nad_deque_copy_to_span(const nad_Deque *self, nad_SpanMut dst) {
    ASSERT_DEQUE(self);
    NAD_SPAN_ASSERT(dst);
    assert(dst.elem_size == self->elem_size);
    assert(dst.len == self->len);

    copy_out(self, dst.data);
}

void nad_deque_copy_from_span(nad_Deque *self, nad_Span src) {
    ASSERT_DEQUE(self);
    NAD_SPAN_ASSERT(src);
    assert(src.elem_size == self->elem_size);
    assert(src.len == self->len);

    copy_in(self, src.data);
}

/* ========== compare ========== */

bool nad_deque_eq(const nad_Deque *a, const nad_Deque *b) {
    ASSERT_DEQUE(a);
    ASSERT_DEQUE(b);
    assert(a->elem_size == b->elem_size);

    if (a == b) {
        return true;
    }

    if (a->len != b->len) {
        return false;
    }

    // by index and not by memcmp over the buffers: two rings holding the same elems start
    // at different slots, and the bytes outside the contents are not contents
    for (size_t i = 0; i < a->len; ++i) {
        if (memcmp(elem_at(a, i), elem_at(b, i), a->elem_size) != 0) {
            return false;
        }
    }

    return true;
}

bool nad_deque_eq_by(const nad_Deque *a, const nad_Deque *b, nad_Eq eq) {
    ASSERT_DEQUE(a);
    ASSERT_DEQUE(b);
    assert(a->elem_size == b->elem_size);
    assert(eq);

    if (a == b) {
        return true;
    }

    if (a->len != b->len) {
        return false;
    }

    for (size_t i = 0; i < a->len; ++i) {
        if (!eq(elem_at(a, i), elem_at(b, i))) {
            return false;
        }
    }

    return true;
}

/* ========== info ========== */

size_t nad_deque_len(const nad_Deque *self) {
    ASSERT_DEQUE(self);

    return self->len;
}

size_t nad_deque_cap(const nad_Deque *self) {
    ASSERT_DEQUE(self);

    return self->cap;
}

size_t nad_deque_elem_size(const nad_Deque *self) {
    ASSERT_DEQUE(self);

    return self->elem_size;
}

size_t nad_deque_bytes(const nad_Deque *self) {
    ASSERT_DEQUE(self);

    return len_bytes(self);
}

nad_Al *nad_deque_al(const nad_Deque *self) {
    ASSERT_DEQUE(self);

    return self->al;
}

/* ========== access ========== */

const void *nad_deque_first(const nad_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    return elem_at(self, 0);
}

void *nad_deque_first_mut(nad_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    return elem_at_mut(self, 0);
}

const void *nad_deque_last(const nad_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    return elem_at(self, self->len - 1);
}

void *nad_deque_last_mut(nad_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    return elem_at_mut(self, self->len - 1);
}

const void *nad_deque_get(const nad_Deque *self, size_t idx) {
    ASSERT_DEQUE(self);
    assert(idx < self->len);

    return elem_at(self, idx);
}

void *nad_deque_get_mut(nad_Deque *self, size_t idx) {
    ASSERT_DEQUE(self);
    assert(idx < self->len);

    return elem_at_mut(self, idx);
}

void nad_deque_set(nad_Deque *self, size_t idx, const void *val) {
    ASSERT_DEQUE(self);
    assert(val);
    assert(idx < self->len);

    memcpy(elem_at_mut(self, idx), val, self->elem_size);
}

/* ========== mods ========== */

nad_Status nad_deque_push_front(nad_Deque *self, const void *val) {
    ASSERT_DEQUE(self);
    assert(val);

    if (self->len == self->cap) {
        const nad_Status st = grow(self);
        if (NAD_STATUS_IS_ERR(st)) {
            return st;
        }
    }

    // the slot before the front, one lap back when the front is slot 0
    self->head = self->head == 0 ? self->cap - 1 : self->head - 1;
    memcpy(slot_at_mut(self, self->head), val, self->elem_size);
    ++self->len;

    return NAD_STATUS_OK;
}

nad_Status nad_deque_push_back(nad_Deque *self, const void *val) {
    ASSERT_DEQUE(self);
    assert(val);

    if (self->len == self->cap) {
        const nad_Status st = grow(self);
        if (NAD_STATUS_IS_ERR(st)) {
            return st;
        }
    }

    memcpy(elem_at_mut(self, self->len), val, self->elem_size);
    ++self->len;

    return NAD_STATUS_OK;
}

void nad_deque_pop_front(nad_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    self->head = self->head + 1 == self->cap ? 0 : self->head + 1;
    --self->len;
}

void nad_deque_pop_back(nad_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    --self->len;
}

nad_Status nad_deque_insert(nad_Deque *self, size_t idx, const void *val) {
    ASSERT_DEQUE(self);
    assert(val);
    assert(idx <= self->len);

    if (self->len == self->cap) {
        const nad_Status st = grow(self);
        if (NAD_STATUS_IS_ERR(st)) {
            return st;
        }
    }

    // whichever side is shorter gets shifted. The two sides move in opposite
    // directions, so the loops run the opposite way round as well
    if (idx <= self->len / 2) {
        self->head = self->head == 0 ? self->cap - 1 : self->head - 1;
        ++self->len;

        // every elem before 'idx' is now one place too far back
        for (size_t i = 0; i < idx; ++i) {
            move_elem(self, i, i + 1);
        }
    } else {
        ++self->len;

        for (size_t i = self->len - 1; i > idx; --i) {
            move_elem(self, i, i - 1);
        }
    }

    memcpy(elem_at_mut(self, idx), val, self->elem_size);

    return NAD_STATUS_OK;
}

void nad_deque_remove(nad_Deque *self, size_t idx) {
    ASSERT_DEQUE(self);
    assert(idx < self->len);

    if (idx < self->len - idx - 1) {
        // the front side is shorter: slide it forward over the hole
        for (size_t i = idx; i > 0; --i) {
            move_elem(self, i, i - 1);
        }
        self->head = self->head + 1 == self->cap ? 0 : self->head + 1;
    } else {
        for (size_t i = idx; i + 1 < self->len; ++i) {
            move_elem(self, i, i + 1);
        }
    }

    --self->len;
}

void nad_deque_clear(nad_Deque *self) {
    ASSERT_DEQUE(self);

    self->len = 0;
    self->head = 0;
}

nad_Status nad_deque_reserve(nad_Deque *self, size_t new_cap) {
    ASSERT_DEQUE(self);

    if (new_cap <= self->cap) {
        return NAD_STATUS_OK;
    }

    size_t new_bytes;
    if (ckd_mul(&new_bytes, new_cap, self->elem_size)) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    // a fresh block rather than a realloc: growing in place would leave the wrapped
    // part sitting before the seam, where the new capacity does not reach it. The
    // contents are unrolled into the new block instead, which is the same two memcpy
    // a realloc would have needed anyway
    void *data = nad_alloc(self->al, new_bytes);
    if (!data) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    copy_out(self, data);
    nad_dealloc(self->al, self->data, cap_bytes(self));

    self->data = data;
    self->cap = new_cap;
    self->head = 0;

    ASSERT_DEQUE(self);

    return NAD_STATUS_OK;
}

nad_Status nad_deque_shrink_to_fit(nad_Deque *self) {
    ASSERT_DEQUE(self);

    if (self->len == self->cap) {
        return NAD_STATUS_OK;
    }

    if (self->len == 0) {
        nad_dealloc(self->al, self->data, cap_bytes(self));
        self->data = nullptr;
        self->cap = 0;
        self->head = 0;

        ASSERT_DEQUE(self);

        return NAD_STATUS_OK;
    }

    void *data = nad_alloc(self->al, len_bytes(self));
    if (!data) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    copy_out(self, data);
    nad_dealloc(self->al, self->data, cap_bytes(self));

    self->data = data;
    self->cap = self->len;
    self->head = 0;

    ASSERT_DEQUE(self);

    return NAD_STATUS_OK;
}

nad_Status nad_deque_resize(nad_Deque *self, size_t new_len) {
    ASSERT_DEQUE(self);

    if (new_len <= self->len) {
        self->len = new_len;
        return NAD_STATUS_OK;
    }

    if (new_len > self->cap) {
        const nad_Status st = nad_deque_reserve(self, new_len);
        if (NAD_STATUS_IS_ERR(st)) {
            return st;
        }
    }

    // the new tail may straddle the seam, so it is zeroed a slot at a time rather
    // than with one memset over a range that need not be contiguous
    for (size_t i = self->len; i < new_len; ++i) {
        memset(elem_at_mut(self, i), 0, self->elem_size);
    }
    self->len = new_len;

    ASSERT_DEQUE(self);

    return NAD_STATUS_OK;
}

nad_Status nad_deque_swap(nad_Deque *self, nad_Deque *other) {
    ASSERT_DEQUE(self);
    ASSERT_DEQUE(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    // one allocator on both sides: the buffers are handed over, ring and all
    if (self->al == other->al) {
        NAD_SWAP(*self, *other);
        return NAD_STATUS_OK;
    }

    // two allocators: neither may free the other's memory, so the bytes are moved.
    // Each side is sized to the content it receives and comes out unwrapped, exactly
    // as in nad_deque_copy.
    const size_t self_bytes = len_bytes(self);
    const size_t other_bytes = len_bytes(other);

    void *self_new = nullptr;
    void *other_new = nullptr;

    if (other_bytes > 0) {
        self_new = nad_alloc(self->al, other_bytes);
        if (!self_new) {
            return NAD_STATUS_ERR_NO_MEM;
        }
    }

    if (self_bytes > 0) {
        other_new = nad_alloc(other->al, self_bytes);
        if (!other_new) {
            nad_dealloc(self->al, self_new, other_bytes);
            return NAD_STATUS_ERR_NO_MEM;
        }
    }

    if (other_bytes > 0) {
        copy_out(other, self_new);
    }

    if (self_bytes > 0) {
        copy_out(self, other_new);
    }

    // the old blocks are handed back at their allocated size, not their used one
    nad_dealloc(self->al, self->data, cap_bytes(self));
    nad_dealloc(other->al, other->data, cap_bytes(other));

    self->data = self_new;
    other->data = other_new;
    NAD_SWAP(self->len, other->len);
    self->cap = self->len;
    other->cap = other->len;
    self->head = 0;
    other->head = 0;

    ASSERT_DEQUE(self);
    ASSERT_DEQUE(other);

    return NAD_STATUS_OK;
}

void nad_deque_swap_elems(nad_Deque *self, size_t i, size_t j) {
    ASSERT_DEQUE(self);
    assert(i < self->len);
    assert(j < self->len);

    if (i == j) {
        return;
    }

    nad_bytes_swap(elem_at_mut(self, i), elem_at_mut(self, j), self->elem_size);
}

/* ========== print ========== */

void nad_deque_fprint(const nad_Deque *self, FILE *stream, nad_FPrint fprint) {
    ASSERT_DEQUE(self);
    assert(stream);
    assert(fprint);

    // spelled out rather than delegated to nad_span_fprint: there is no span to
    // delegate with. The format is the same one on purpose
    fputc('[', stream);
    for (size_t i = 0; i < self->len; ++i) {
        if (i > 0) {
            fputs(", ", stream);
        }
        fprint(stream, elem_at(self, i));
    }
    fputs("]\n", stream);
}

void nad_deque_print(const nad_Deque *self, nad_FPrint fprint) {
    ASSERT_DEQUE(self);
    assert(fprint);

    nad_deque_fprint(self, stdout, fprint);
}

/* ========== internals ========== */

static nad_Status new_impl(bool zeroed, size_t len, size_t cap, size_t elem_size, nad_Al *al, nad_Deque **out) {
    assert(len <= cap);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Deque *obj = nad_alloc(al, sizeof(nad_Deque));
    if (!obj) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    void *data = nullptr;

    if (cap > 0) {
        size_t bytes;
        if (ckd_mul(&bytes, cap, elem_size)) {
            goto fail;
        }
        data = nad_alloc(al, bytes);
        if (!data) {
            goto fail;
        }
        if (zeroed) {
            memset(data, 0, len * elem_size);
        }
    }

    set_fields(obj, data, len, cap, elem_size, al);

    ASSERT_DEQUE(obj);

    *out = obj;
    return NAD_STATUS_OK;

fail:
    nad_dealloc(al, obj, sizeof(nad_Deque));
    return NAD_STATUS_ERR_NO_MEM;
}

static void set_fields(nad_Deque *obj, void *data, size_t len, size_t cap, size_t elem_size, nad_Al *al) {
    obj->data = data;
    obj->len = len;
    obj->cap = cap;
    obj->head = 0;
    obj->elem_size = elem_size;
    obj->al = al;
}

static size_t next_cap(const nad_Deque *self) {
    if (self->cap == 0) {
        return DEQUE_GROWTH_BASE;
    }

    size_t grown;
    if (ckd_mul(&grown, self->cap, DEQUE_GROWTH_FACTOR)) {
        return SIZE_MAX;
    }

    return grown;
}

static nad_Status grow(nad_Deque *self) {
    assert(self->len == self->cap);

    if (self->cap == SIZE_MAX) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    const size_t wanted = next_cap(self);

    const nad_Status st = nad_deque_reserve(self, wanted);
    if (NAD_STATUS_IS_OK(st) || wanted <= self->cap + 1) {
        return st;
    }

    return nad_deque_reserve(self, self->cap + 1);
}

static size_t len_bytes(const nad_Deque *self) {
    return self->len * self->elem_size;
}

static size_t cap_bytes(const nad_Deque *self) {
    return self->cap * self->elem_size;
}

static size_t slot_of(const nad_Deque *self, size_t idx) {
    assert(self->cap > 0);
    assert(idx < self->cap);

    const size_t raw = self->head + idx;

    return raw < self->cap ? raw : raw - self->cap;
}

static const unsigned char *slot_at(const nad_Deque *self, size_t slot) {
    return nad_byte_offset(self->data, self->elem_size, slot);
}

static unsigned char *slot_at_mut(nad_Deque *self, size_t slot) {
    return nad_byte_offset_mut(self->data, self->elem_size, slot);
}

static const unsigned char *elem_at(const nad_Deque *self, size_t idx) {
    return slot_at(self, slot_of(self, idx));
}

static unsigned char *elem_at_mut(nad_Deque *self, size_t idx) {
    return slot_at_mut(self, slot_of(self, idx));
}

static size_t first_run(const nad_Deque *self) {
    const size_t to_end = self->cap - self->head;

    return to_end < self->len ? to_end : self->len;
}

static void copy_out(const nad_Deque *self, void *dst) {
    if (self->len == 0) {
        return;
    }

    const size_t run = first_run(self);
    memcpy(dst, slot_at(self, self->head), run * self->elem_size);

    if (run < self->len) {
        memcpy(
            nad_byte_offset_mut(dst, self->elem_size, run),
            slot_at(self, 0),
            (self->len - run) * self->elem_size
        );
    }
}

static void copy_in(nad_Deque *self, const void *src) {
    if (self->len == 0) {
        return;
    }

    const size_t run = first_run(self);
    memcpy(slot_at_mut(self, self->head), src, run * self->elem_size);

    if (run < self->len) {
        memcpy(
            slot_at_mut(self, 0),
            nad_byte_offset(src, self->elem_size, run),
            (self->len - run) * self->elem_size
        );
    }
}

static void move_elem(nad_Deque *self, size_t to, size_t from) {
    // two distinct slots of the same ring, so they never overlap
    memcpy(elem_at_mut(self, to), elem_at_mut(self, from), self->elem_size);
}
