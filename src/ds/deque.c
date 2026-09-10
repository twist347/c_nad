#include "trs/ds/deque.h"

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

struct trs_Deque {
    void *data;
    size_t len;
    size_t cap;
    // slot holding the front elem. Kept alongside 'len' rather than a tail index:
    // with two indices a full ring and an empty one look exactly alike
    size_t head;
    size_t elem_size;
    trs_Al *al;
};

[[nodiscard]]
static trs_Status new_impl(bool zeroed, size_t len, size_t cap, size_t elem_size, trs_Al *al, trs_Deque **out);

static void set_fields(trs_Deque *obj, void *data, size_t len, size_t cap, size_t elem_size, trs_Al *al);

/// hands the block back and leaves an empty deque on the same allocator
static void release_data(trs_Deque *self);

[[nodiscard]]
static size_t next_cap(const trs_Deque *self);

[[nodiscard]]
static trs_Status grow(trs_Deque *self);

/// room for one more elem, growing the ring when it is full
[[nodiscard]]
static trs_Status reserve_one(trs_Deque *self);

[[nodiscard]]
static size_t len_bytes(const trs_Deque *self);

[[nodiscard]]
static size_t cap_bytes(const trs_Deque *self);

/// the slot holding the elem at 'idx' counted from the front. One subtraction and no
/// division: 'idx' is below cap, so head + idx overshoots by less than one lap
[[nodiscard]]
static size_t slot_of(const trs_Deque *self, size_t idx);

[[nodiscard]]
static const unsigned char *slot_at(const trs_Deque *self, size_t slot);

[[nodiscard]]
static unsigned char *slot_at_mut(trs_Deque *self, size_t slot);

[[nodiscard]]
static const unsigned char *elem_at(const trs_Deque *self, size_t idx);

[[nodiscard]]
static unsigned char *elem_at_mut(trs_Deque *self, size_t idx);

/// how many elems sit between head and the end of the buffer — the length of the first
/// of the at most two runs the contents form
[[nodiscard]]
static size_t first_run(const trs_Deque *self);

/// every elem, in ring order, into 'dst' bytes. The caller guarantees the room
static void copy_out(const trs_Deque *self, void *dst);

/// the inverse: 'src' bytes over every elem, in ring order
static void copy_in(trs_Deque *self, const void *src);

static void move_elem(trs_Deque *self, size_t to, size_t from);

/* ========== lifetime ========== */

trs_Status trs_deque_new(size_t elem_size, trs_Al *al, trs_Deque **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(false, 0, 0, elem_size, al, out);
}

trs_Status trs_deque_new_len(size_t len, size_t elem_size, trs_Al *al, trs_Deque **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(true, len, len, elem_size, al, out);
}

trs_Status trs_deque_new_cap(size_t cap, size_t elem_size, trs_Al *al, trs_Deque **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(false, 0, cap, elem_size, al, out);
}

trs_Status trs_deque_from_data(const void *data, size_t len, size_t elem_size, trs_Al *al, trs_Deque **out) {
    assert(data || len == 0);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Deque *deque;
    const trs_Status st = new_impl(false, len, len, elem_size, al, &deque);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    // head is 0 here, so the ring starts out unwrapped and this is one memcpy
    if (len > 0) {
        memcpy(deque->data, data, len_bytes(deque));
    }

    *out = deque;

    return TRS_STATUS_OK;
}

trs_Status trs_deque_from_span(trs_Span s, trs_Al *al, trs_Deque **out) {
    TRS_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return trs_deque_from_data(s.data, s.len, s.elem_size, al, out);
}

void trs_deque_drop(trs_Deque *self) {
    if (!self) {
        return;
    }

    ASSERT_DEQUE(self);

    trs_Al *al_copy = self->al;
    trs_dealloc(al_copy, self->data, cap_bytes(self));
    trs_dealloc(al_copy, self, sizeof(trs_Deque));
}

/* ========== copy ========== */

trs_Status trs_deque_copy(const trs_Deque *self, trs_Deque **out) {
    ASSERT_DEQUE(self);

    return trs_deque_copy_with(self, self->al, out);
}

trs_Status trs_deque_copy_with(const trs_Deque *self, trs_Al *al, trs_Deque **out) {
    ASSERT_DEQUE(self);
    assert(al);
    assert(out);

    trs_Deque *copy;
    const trs_Status st = new_impl(false, self->len, self->len, self->elem_size, al, &copy);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    // the copy is sized to the content, so it comes out unwrapped whatever self looks like
    copy_out(self, copy->data);

    *out = copy;

    return TRS_STATUS_OK;
}

trs_Status trs_deque_copy_assign(const trs_Deque *self, trs_Deque *other) {
    ASSERT_DEQUE(self);
    ASSERT_DEQUE(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    const trs_Status st = trs_deque_reserve(other, self->len);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    // whatever 'other' held is gone, so its ring is laid out afresh from slot 0
    other->head = 0;
    other->len = self->len;
    copy_out(self, other->data);

    ASSERT_DEQUE(other);

    return TRS_STATUS_OK;
}

trs_Status trs_deque_move_assign(trs_Deque *self, trs_Deque *other) {
    ASSERT_DEQUE(self);
    ASSERT_DEQUE(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    // one allocator: the block is handed over, ring and all. What 'other' held ends up in 'self' and is released
    // there, through the very allocator that made it
    if (self->al == other->al) {
        TRS_SWAP(*self, *other);
        release_data(self);

        ASSERT_DEQUE(self);
        ASSERT_DEQUE(other);

        return TRS_STATUS_OK;
    }

    // two allocators: the whole copy is built on the target's before anything of it is
    // touched, so a refusal leaves both as they were
    trs_Deque *obj;
    const trs_Status st = trs_deque_copy_with(self, other->al, &obj);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    TRS_SWAP(*other, *obj);
    trs_deque_drop(obj);
    release_data(self);

    ASSERT_DEQUE(self);
    ASSERT_DEQUE(other);

    return TRS_STATUS_OK;
}

void trs_deque_copy_to_span(const trs_Deque *self, trs_SpanMut dst) {
    ASSERT_DEQUE(self);
    TRS_SPAN_ASSERT(dst);
    assert(dst.elem_size == self->elem_size);
    assert(dst.len == self->len);

    copy_out(self, dst.data);
}

void trs_deque_copy_from_span(trs_Deque *self, trs_Span src) {
    ASSERT_DEQUE(self);
    TRS_SPAN_ASSERT(src);
    assert(src.elem_size == self->elem_size);
    assert(src.len == self->len);

    copy_in(self, src.data);
}

/* ========== compare ========== */

bool trs_deque_eq(const trs_Deque *a, const trs_Deque *b) {
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

bool trs_deque_eq_by(const trs_Deque *a, const trs_Deque *b, trs_Eq eq) {
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

size_t trs_deque_len(const trs_Deque *self) {
    ASSERT_DEQUE(self);

    return self->len;
}

size_t trs_deque_cap(const trs_Deque *self) {
    ASSERT_DEQUE(self);

    return self->cap;
}

size_t trs_deque_elem_size(const trs_Deque *self) {
    ASSERT_DEQUE(self);

    return self->elem_size;
}

size_t trs_deque_bytes(const trs_Deque *self) {
    ASSERT_DEQUE(self);

    return len_bytes(self);
}

trs_Al *trs_deque_al(const trs_Deque *self) {
    ASSERT_DEQUE(self);

    return self->al;
}

/* ========== access ========== */

const void *trs_deque_front(const trs_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    return elem_at(self, 0);
}

void *trs_deque_front_mut(trs_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    return elem_at_mut(self, 0);
}

const void *trs_deque_back(const trs_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    return elem_at(self, self->len - 1);
}

void *trs_deque_back_mut(trs_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    return elem_at_mut(self, self->len - 1);
}

const void *trs_deque_get(const trs_Deque *self, size_t idx) {
    ASSERT_DEQUE(self);
    assert(idx < self->len);

    return elem_at(self, idx);
}

void *trs_deque_get_mut(trs_Deque *self, size_t idx) {
    ASSERT_DEQUE(self);
    assert(idx < self->len);

    return elem_at_mut(self, idx);
}

void trs_deque_set(trs_Deque *self, size_t idx, const void *val) {
    ASSERT_DEQUE(self);
    assert(val);
    assert(idx < self->len);

    memcpy(elem_at_mut(self, idx), val, self->elem_size);
}

/* ========== mods ========== */

trs_Status trs_deque_push_front(trs_Deque *self, const void *val) {
    ASSERT_DEQUE(self);
    assert(val);

    const trs_Status st = reserve_one(self);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    // the slot before the front, one lap back when the front is slot 0
    self->head = self->head == 0 ? self->cap - 1 : self->head - 1;
    memcpy(slot_at_mut(self, self->head), val, self->elem_size);
    ++self->len;

    return TRS_STATUS_OK;
}

trs_Status trs_deque_push_back(trs_Deque *self, const void *val) {
    ASSERT_DEQUE(self);
    assert(val);

    const trs_Status st = reserve_one(self);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    memcpy(elem_at_mut(self, self->len), val, self->elem_size);
    ++self->len;

    return TRS_STATUS_OK;
}

void trs_deque_pop_front(trs_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    self->head = self->head + 1 == self->cap ? 0 : self->head + 1;
    --self->len;
}

void trs_deque_pop_back(trs_Deque *self) {
    ASSERT_DEQUE(self);
    assert(self->len > 0);

    --self->len;
}

trs_Status trs_deque_insert(trs_Deque *self, size_t idx, const void *val) {
    ASSERT_DEQUE(self);
    assert(val);
    assert(idx <= self->len);

    const trs_Status st = reserve_one(self);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
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

    return TRS_STATUS_OK;
}

void trs_deque_remove(trs_Deque *self, size_t idx) {
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

void trs_deque_clear(trs_Deque *self) {
    ASSERT_DEQUE(self);

    self->len = 0;
    self->head = 0;
}

trs_Status trs_deque_reserve(trs_Deque *self, size_t new_cap) {
    ASSERT_DEQUE(self);

    if (new_cap <= self->cap) {
        return TRS_STATUS_OK;
    }

    size_t new_bytes;
    if (ckd_mul(&new_bytes, new_cap, self->elem_size)) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    // a fresh block rather than a realloc: growing in place would leave the wrapped
    // part sitting before the seam, where the new capacity does not reach it. The
    // contents are unrolled into the new block instead, which is the same two memcpy
    // a realloc would have needed anyway
    void *data = trs_alloc(self->al, new_bytes);
    if (!data) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    copy_out(self, data);
    trs_dealloc(self->al, self->data, cap_bytes(self));

    self->data = data;
    self->cap = new_cap;
    self->head = 0;

    ASSERT_DEQUE(self);

    return TRS_STATUS_OK;
}

trs_Status trs_deque_shrink_to_fit(trs_Deque *self) {
    ASSERT_DEQUE(self);

    if (self->len == self->cap) {
        return TRS_STATUS_OK;
    }

    if (self->len == 0) {
        trs_dealloc(self->al, self->data, cap_bytes(self));
        self->data = nullptr;
        self->cap = 0;
        self->head = 0;

        ASSERT_DEQUE(self);

        return TRS_STATUS_OK;
    }

    void *data = trs_alloc(self->al, len_bytes(self));
    if (!data) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    copy_out(self, data);
    trs_dealloc(self->al, self->data, cap_bytes(self));

    self->data = data;
    self->cap = self->len;
    self->head = 0;

    ASSERT_DEQUE(self);

    return TRS_STATUS_OK;
}

trs_Status trs_deque_resize(trs_Deque *self, size_t new_len) {
    ASSERT_DEQUE(self);

    if (new_len <= self->len) {
        self->len = new_len;
        return TRS_STATUS_OK;
    }

    if (new_len > self->cap) {
        const trs_Status st = trs_deque_reserve(self, new_len);
        if (TRS_STATUS_IS_ERR(st)) {
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

    return TRS_STATUS_OK;
}

void trs_deque_swap(trs_Deque *self, trs_Deque *other) {
    ASSERT_DEQUE(self);
    ASSERT_DEQUE(other);
    assert(self->elem_size == other->elem_size);
    assert(self->al == other->al);

    if (self == other) {
        return;
    }

    TRS_SWAP(*self, *other);

    ASSERT_DEQUE(self);
    ASSERT_DEQUE(other);
}

void trs_deque_swap_elems(trs_Deque *self, size_t i, size_t j) {
    ASSERT_DEQUE(self);
    assert(i < self->len);
    assert(j < self->len);

    if (i == j) {
        return;
    }

    trs_bytes_swap(elem_at_mut(self, i), elem_at_mut(self, j), self->elem_size);
}

/* ========== print ========== */

void trs_deque_fprint(const trs_Deque *self, FILE *stream, trs_FPrint fprint) {
    ASSERT_DEQUE(self);
    assert(stream);
    assert(fprint);

    // spelled out rather than delegated to trs_span_fprint: there is no span to
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

void trs_deque_print(const trs_Deque *self, trs_FPrint fprint) {
    ASSERT_DEQUE(self);
    assert(fprint);

    trs_deque_fprint(self, stdout, fprint);
}

/* ========== internals ========== */

static trs_Status new_impl(bool zeroed, size_t len, size_t cap, size_t elem_size, trs_Al *al, trs_Deque **out) {
    assert(len <= cap);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Deque *obj = trs_alloc(al, sizeof(trs_Deque));
    if (!obj) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    void *data = nullptr;

    if (cap > 0) {
        size_t bytes;
        if (ckd_mul(&bytes, cap, elem_size)) {
            goto fail;
        }
        data = trs_alloc(al, bytes);
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
    return TRS_STATUS_OK;

fail:
    trs_dealloc(al, obj, sizeof(trs_Deque));
    return TRS_STATUS_ERR_NO_MEM;
}

static void set_fields(trs_Deque *obj, void *data, size_t len, size_t cap, size_t elem_size, trs_Al *al) {
    obj->data = data;
    obj->len = len;
    obj->cap = cap;
    obj->head = 0;
    obj->elem_size = elem_size;
    obj->al = al;
}

static size_t next_cap(const trs_Deque *self) {
    if (self->cap == 0) {
        return DEQUE_GROWTH_BASE;
    }

    size_t grown;
    if (ckd_mul(&grown, self->cap, DEQUE_GROWTH_FACTOR)) {
        return SIZE_MAX;
    }

    return grown;
}

static trs_Status reserve_one(trs_Deque *self) {
    return self->len == self->cap ? grow(self) : TRS_STATUS_OK;
}

static trs_Status grow(trs_Deque *self) {
    assert(self->len == self->cap);

    if (self->cap == SIZE_MAX) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    const size_t wanted = next_cap(self);

    const trs_Status st = trs_deque_reserve(self, wanted);
    if (TRS_STATUS_IS_OK(st) || wanted <= self->cap + 1) {
        return st;
    }

    return trs_deque_reserve(self, self->cap + 1);
}

static void release_data(trs_Deque *self) {
    trs_dealloc(self->al, self->data, cap_bytes(self));
    self->data = nullptr;
    self->len = 0;
    self->cap = 0;
    self->head = 0;
}

static size_t len_bytes(const trs_Deque *self) {
    return self->len * self->elem_size;
}

static size_t cap_bytes(const trs_Deque *self) {
    return self->cap * self->elem_size;
}

static size_t slot_of(const trs_Deque *self, size_t idx) {
    assert(self->cap > 0);
    assert(idx < self->cap);

    const size_t raw = self->head + idx;

    return raw < self->cap ? raw : raw - self->cap;
}

static const unsigned char *slot_at(const trs_Deque *self, size_t slot) {
    return trs_byte_offset(self->data, self->elem_size, slot);
}

static unsigned char *slot_at_mut(trs_Deque *self, size_t slot) {
    return trs_byte_offset_mut(self->data, self->elem_size, slot);
}

static const unsigned char *elem_at(const trs_Deque *self, size_t idx) {
    return slot_at(self, slot_of(self, idx));
}

static unsigned char *elem_at_mut(trs_Deque *self, size_t idx) {
    return slot_at_mut(self, slot_of(self, idx));
}

static size_t first_run(const trs_Deque *self) {
    const size_t to_end = self->cap - self->head;

    return to_end < self->len ? to_end : self->len;
}

static void copy_out(const trs_Deque *self, void *dst) {
    if (self->len == 0) {
        return;
    }

    const size_t run = first_run(self);
    memcpy(dst, slot_at(self, self->head), run * self->elem_size);

    if (run < self->len) {
        memcpy(
            trs_byte_offset_mut(dst, self->elem_size, run),
            slot_at(self, 0),
            (self->len - run) * self->elem_size
        );
    }
}

static void copy_in(trs_Deque *self, const void *src) {
    if (self->len == 0) {
        return;
    }

    const size_t run = first_run(self);
    memcpy(slot_at_mut(self, self->head), src, run * self->elem_size);

    if (run < self->len) {
        memcpy(
            slot_at_mut(self, 0),
            trs_byte_offset(src, self->elem_size, run),
            (self->len - run) * self->elem_size
        );
    }
}

static void move_elem(trs_Deque *self, size_t to, size_t from) {
    // two distinct slots of the same ring, so they never overlap
    memcpy(elem_at_mut(self, to), elem_at_mut(self, from), self->elem_size);
}
