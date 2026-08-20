#include "nad/ds/vec.h"
#include "nad/alloc/arena.h"
#include "nad/alloc/default.h"

#include "support/arena.h"

#include "unity.h"

#include <stddef.h>
#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// an elem wider than a word, to keep elem_size honest
typedef struct {
    int64_t a;
    int64_t b;
} Pair;

// the arena hands out nothing finer than this, so a request is charged rounded up
static constexpr size_t ARENA_STEP = alignof(max_align_t);

static size_t arena_charge(size_t bytes) {
    return (bytes + ARENA_STEP - 1) / ARENA_STEP * ARENA_STEP;
}

// int32_t vec of len elems holding 0, 1, ... len-1; cap == len
static nad_Vec *make_vec(size_t len) {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW_LEN(int32_t, len, nad_al_default(), &v));

    for (size_t i = 0; i < len; ++i) {
        NAD_VEC_SET(int32_t, v, i, (int32_t) i);
    }
    return v;
}

// empty int32_t vec with room for cap elems
static nad_Vec *make_vec_cap(size_t cap) {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW_CAP(int32_t, cap, nad_al_default(), &v));

    return v;
}

static void push_int(nad_Vec *v, int32_t val) {
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_push(v, &val));
}

/* ========== lifetime ========== */

static void test_new_starts_empty_and_unallocated() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW(int32_t, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(0, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_vec_elem_size(v));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_vec_al(v));
    TEST_ASSERT_NULL(nad_vec_data(v));

    nad_vec_drop(v);
}

static void test_new_len_sets_len_cap_and_zeroes() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW_LEN(int32_t, 4, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_size_t(4, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(4, nad_vec_cap(v));

    constexpr int32_t zeroes[4] = {0, 0, 0, 0};
    TEST_ASSERT_EQUAL_INT32_ARRAY(zeroes, nad_vec_data(v), 4);

    nad_vec_drop(v);
}

// cap is room, not content: the vec is still empty
static void test_new_cap_reserves_without_length() {
    nad_Vec *v = make_vec_cap(8);

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(8, nad_vec_cap(v));
    TEST_ASSERT_NOT_NULL(nad_vec_data(v));

    nad_vec_drop(v);
}

static void test_new_cap_zero_has_no_buffer() {
    nad_Vec *v = make_vec_cap(0);

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_cap(v));
    TEST_ASSERT_NULL(nad_vec_data(v));

    nad_vec_drop(v);
}

static void test_from_data_copies_and_detaches_the_source() {
    int32_t src[3] = {5, 6, 7};

    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_FROM_DATA(int32_t, src, 3, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_INT32_ARRAY(src, nad_vec_data(v), 3);
    TEST_ASSERT_TRUE((const void *) src != nad_vec_data(v));

    src[0] = 999;
    TEST_ASSERT_EQUAL_INT32(5, *NAD_VEC_GET_AS(int32_t, v, 0));

    nad_vec_drop(v);
}

// null source is legal while len == 0 — same rule as nad_span_new
static void test_from_data_empty_has_no_buffer() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_vec_from_data(nullptr, 0, sizeof(int32_t), nad_al_default(), &v)
    );

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(0, nad_vec_cap(v));
    TEST_ASSERT_NULL(nad_vec_data(v));

    nad_vec_drop(v);
}

// elem_size drives the copy, so a type wider than a word must arrive whole
static void test_from_data_copies_whole_elems() {
    const Pair src[2] = {{1, 2}, {3, 4}};

    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_FROM_DATA(Pair, src, 2, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), nad_vec_elem_size(v));
    TEST_ASSERT_EQUAL_INT64(1, NAD_VEC_GET_AS(Pair, v, 0)->a);
    TEST_ASSERT_EQUAL_INT64(2, NAD_VEC_GET_AS(Pair, v, 0)->b);
    TEST_ASSERT_EQUAL_INT64(3, NAD_VEC_GET_AS(Pair, v, 1)->a);
    TEST_ASSERT_EQUAL_INT64(4, NAD_VEC_GET_AS(Pair, v, 1)->b);

    nad_vec_drop(v);
}

static void test_from_span_copies_the_view() {
    constexpr int32_t src[3] = {7, 8, 9};

    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_vec_from_span(NAD_SPAN_NEW(int32_t, src, 3), nad_al_default(), &v)
    );

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_vec_elem_size(v));
    TEST_ASSERT_EQUAL_INT32_ARRAY(src, nad_vec_data(v), 3);
    TEST_ASSERT_TRUE((const void *) src != nad_vec_data(v));

    nad_vec_drop(v);
}

static void test_drop_null_is_noop() {
    nad_vec_drop(nullptr);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    nad_Vec *src = make_vec(4);

    nad_Vec *dst = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(4, nad_vec_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_vec_data(src), nad_vec_data(dst), 4);
    TEST_ASSERT_TRUE(nad_vec_data(src) != nad_vec_data(dst));

    NAD_VEC_SET(int32_t, src, 0, 999);
    TEST_ASSERT_EQUAL_INT32(0, *NAD_VEC_GET_AS(int32_t, dst, 0));

    nad_vec_drop(dst);
    nad_vec_drop(src);
}

// the copy is sized to the content, so the source's spare capacity is not carried over
static void test_copy_fits_the_len_not_the_source_capacity() {
    nad_Vec *src = make_vec_cap(8);
    push_int(src, 1);
    push_int(src, 2);

    nad_Vec *dst = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(dst));
    TEST_ASSERT_EQUAL_size_t(2, nad_vec_cap(dst));

    nad_vec_drop(dst);
    nad_vec_drop(src);
}

static void test_copy_of_empty_has_no_buffer() {
    nad_Vec *src = make_vec(0);

    nad_Vec *dst = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(dst));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_vec_elem_size(dst));
    TEST_ASSERT_NULL(nad_vec_data(dst));

    nad_vec_drop(dst);
    nad_vec_drop(src);
}

static void test_copy_inherits_the_source_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Vec *src = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_OF(int32_t, arena, &src, 1, 2, 3));

    nad_Vec *dst = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, nad_vec_al(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_vec_data(src), nad_vec_data(dst), 3);

    nad_vec_drop(dst);
    nad_vec_drop(src);
    nad_al_arena_drop(arena);
}

static void test_copy_assign_grows_and_shrinks_the_len() {
    nad_Vec *src = make_vec(6);
    nad_Vec *dst = make_vec(2);

    // grow: 2 -> 6
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_copy_assign(src, dst));
    TEST_ASSERT_EQUAL_size_t(6, nad_vec_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_vec_data(src), nad_vec_data(dst), 6);

    // shrink: 6 -> 3
    nad_Vec *small = make_vec(3);
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_copy_assign(small, dst));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_vec_data(small), nad_vec_data(dst), 3);

    // shrink to empty: the len goes, the buffer stays
    nad_Vec *empty = make_vec(0);
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_copy_assign(empty, dst));
    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(dst));
    TEST_ASSERT_NOT_NULL(nad_vec_data(dst));

    nad_vec_drop(empty);
    nad_vec_drop(small);
    nad_vec_drop(dst);
    nad_vec_drop(src);
}

// unlike arr, assignment never gives capacity back — that is what shrink_to_fit is for
static void test_copy_assign_keeps_the_target_capacity() {
    nad_Vec *src = make_vec(2);
    nad_Vec *dst = make_vec(6);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(dst));
    TEST_ASSERT_EQUAL_size_t(6, nad_vec_cap(dst));

    nad_vec_drop(dst);
    nad_vec_drop(src);
}

static void test_copy_assign_self_is_noop() {
    nad_Vec *v = make_vec(3);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_copy_assign(v, v));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_VEC_GET_AS(int32_t, v, 2));

    nad_vec_drop(v);
}

// assignment resizes through the target's allocator, not the source's
static void test_copy_assign_keeps_the_target_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Vec *src = make_vec(4);

    nad_Vec *dst = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW_LEN(int32_t, 1, arena, &dst));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(4, nad_vec_len(dst));
    TEST_ASSERT_EQUAL_PTR(arena, nad_vec_al(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_vec_data(src), nad_vec_data(dst), 4);

    nad_vec_drop(dst);
    nad_vec_drop(src);
    nad_al_arena_drop(arena);
}

/* ========== info ========== */

static void test_bytes_is_len_times_elem_size() {
    nad_Vec *v = make_vec(4);

    TEST_ASSERT_EQUAL_size_t(4 * sizeof(int32_t), nad_vec_bytes(v));

    nad_vec_drop(v);
}

// bytes measures the content, not the allocation — spare capacity does not count
static void test_bytes_ignores_the_spare_capacity() {
    nad_Vec *v = make_vec_cap(8);
    push_int(v, 1);
    push_int(v, 2);
    push_int(v, 3);

    TEST_ASSERT_EQUAL_size_t(8, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_size_t(3 * sizeof(int32_t), nad_vec_bytes(v));

    nad_vec_drop(v);
}

static void test_bytes_of_empty_is_zero() {
    nad_Vec *v = make_vec_cap(8);

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_bytes(v));

    nad_vec_drop(v);
}

// elem_size, not the elem count, drives the total
static void test_bytes_tracks_elem_size() {
    const Pair src[2] = {{1, 2}, {3, 4}};

    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_FROM_DATA(Pair, src, 2, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_size_t(2 * sizeof(Pair), nad_vec_bytes(v));

    nad_vec_drop(v);
}

// the vec and its view must agree on the size of the same memory
static void test_bytes_agrees_with_the_span() {
    nad_Vec *v = make_vec_cap(8);
    push_int(v, 1);
    push_int(v, 2);

    TEST_ASSERT_EQUAL_size_t(nad_span_bytes(nad_vec_to_span(v)), nad_vec_bytes(v));

    nad_vec_drop(v);
}

/* ========== access ========== */

static void test_set_get_roundtrip() {
    nad_Vec *v = make_vec(5);

    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_INT32((int32_t) i, *NAD_VEC_GET_AS(int32_t, v, i));
    }

    nad_vec_drop(v);
}

static void test_get_mut_writes_through() {
    nad_Vec *v = make_vec(3);

    *NAD_VEC_GET_MUT_AS(int32_t, v, 1) = 42;
    TEST_ASSERT_EQUAL_INT32(42, *NAD_VEC_GET_AS(int32_t, v, 1));

    nad_vec_drop(v);
}

static void test_first_and_last_address_the_ends() {
    nad_Vec *v = make_vec(4);

    TEST_ASSERT_EQUAL_INT32(0, *NAD_VEC_FIRST_AS(int32_t, v));
    TEST_ASSERT_EQUAL_INT32(3, *NAD_VEC_LAST_AS(int32_t, v));
    TEST_ASSERT_EQUAL_PTR(nad_vec_data(v), nad_vec_first(v));
    TEST_ASSERT_EQUAL_PTR(NAD_VEC_GET_AS(int32_t, v, 3), nad_vec_last(v));

    nad_vec_drop(v);
}

static void test_first_and_last_mut_write_through() {
    nad_Vec *v = make_vec(4);

    *NAD_VEC_FIRST_MUT_AS(int32_t, v) = 10;
    *NAD_VEC_LAST_MUT_AS(int32_t, v) = 20;

    TEST_ASSERT_EQUAL_INT32(10, *NAD_VEC_GET_AS(int32_t, v, 0));
    TEST_ASSERT_EQUAL_INT32(20, *NAD_VEC_GET_AS(int32_t, v, 3));

    nad_vec_drop(v);
}

static void test_first_and_last_coincide_on_a_single_elem() {
    nad_Vec *v = make_vec(1);

    TEST_ASSERT_EQUAL_PTR(nad_vec_first(v), nad_vec_last(v));
    TEST_ASSERT_EQUAL_PTR(nad_vec_first_mut(v), nad_vec_last_mut(v));

    nad_vec_drop(v);
}

// last follows the len, not the capacity
static void test_last_follows_the_len() {
    nad_Vec *v = make_vec_cap(8);
    push_int(v, 1);
    push_int(v, 2);

    TEST_ASSERT_EQUAL_INT32(2, *NAD_VEC_LAST_AS(int32_t, v));
    TEST_ASSERT_EQUAL_PTR(NAD_VEC_GET_AS(int32_t, v, 1), nad_vec_last(v));

    nad_vec_drop(v);
}

static void test_data_mut_writes_through() {
    nad_Vec *v = make_vec(3);

    int32_t *d = nad_vec_data_mut(v);
    d[2] = 99;

    TEST_ASSERT_EQUAL_INT32(99, *NAD_VEC_GET_AS(int32_t, v, 2));

    nad_vec_drop(v);
}

/* ========== push / pop ========== */

// growth is 0 -> 1 and then doubling, and it only fires when the vec is full
static void test_push_appends_and_doubles_the_capacity() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW(int32_t, nad_al_default(), &v));

    constexpr size_t want_cap[5] = {1, 2, 4, 4, 8};
    for (size_t i = 0; i < 5; ++i) {
        push_int(v, (int32_t) i);

        TEST_ASSERT_EQUAL_size_t(i + 1, nad_vec_len(v));
        TEST_ASSERT_EQUAL_size_t(want_cap[i], nad_vec_cap(v));
    }

    constexpr int32_t want[5] = {0, 1, 2, 3, 4};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 5);

    nad_vec_drop(v);
}

// reserved room is used as is: no growth, no move
static void test_push_uses_reserved_capacity_without_reallocating() {
    nad_Vec *v = make_vec_cap(4);
    const void *before = nad_vec_data(v);

    for (int32_t i = 0; i < 4; ++i) {
        push_int(v, i);
    }

    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));
    TEST_ASSERT_EQUAL_size_t(4, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(4, nad_vec_cap(v));

    nad_vec_drop(v);
}

static void test_push_moves_whole_elems() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW(Pair, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_push(v, &(Pair){1, 2}));
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_push(v, &(Pair){3, 4}));

    TEST_ASSERT_EQUAL_INT64(1, NAD_VEC_GET_AS(Pair, v, 0)->a);
    TEST_ASSERT_EQUAL_INT64(2, NAD_VEC_GET_AS(Pair, v, 0)->b);
    TEST_ASSERT_EQUAL_INT64(3, NAD_VEC_GET_AS(Pair, v, 1)->a);
    TEST_ASSERT_EQUAL_INT64(4, NAD_VEC_GET_AS(Pair, v, 1)->b);

    nad_vec_drop(v);
}

static void test_pop_shortens_and_keeps_the_capacity() {
    nad_Vec *v = make_vec(3);
    const size_t cap = nad_vec_cap(v);

    nad_vec_pop(v);

    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(cap, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_INT32(1, *NAD_VEC_LAST_AS(int32_t, v));

    nad_vec_pop(v);
    nad_vec_pop(v);
    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(v));

    nad_vec_drop(v);
}

// the slot a pop released is the one the next push takes
static void test_push_after_pop_overwrites_the_slot() {
    nad_Vec *v = make_vec(3);
    const void *before = nad_vec_data(v);

    nad_vec_pop(v);
    push_int(v, 77);

    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_INT32(77, *NAD_VEC_GET_AS(int32_t, v, 2));

    nad_vec_drop(v);
}

/* ========== insert / remove ========== */

static void test_insert_at_front_middle_and_end() {
    nad_Vec *v = make_vec(3); // 0, 1, 2

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_INSERT(int32_t, v, 0, 10));
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_INSERT(int32_t, v, 2, 20));
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_INSERT(int32_t, v, 5, 30));

    constexpr int32_t want[6] = {10, 0, 20, 1, 2, 30};
    TEST_ASSERT_EQUAL_size_t(6, nad_vec_len(v));
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 6);

    nad_vec_drop(v);
}

// idx == len on an empty vec is the only legal index there
static void test_insert_into_an_empty_vec() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW(int32_t, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_INSERT(int32_t, v, 0, 42));

    TEST_ASSERT_EQUAL_size_t(1, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(1, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_INT32(42, *NAD_VEC_GET_AS(int32_t, v, 0));

    nad_vec_drop(v);
}

// a full vec must grow before the tail is shifted, or the shift writes past the buffer
static void test_insert_grows_when_full() {
    nad_Vec *v = make_vec(2); // len == cap == 2

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_INSERT(int32_t, v, 0, 9));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_TRUE(nad_vec_cap(v) >= 3);

    constexpr int32_t want[3] = {9, 0, 1};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 3);

    nad_vec_drop(v);
}

static void test_remove_from_front_middle_and_end() {
    nad_Vec *v = make_vec(5); // 0, 1, 2, 3, 4

    nad_vec_remove(v, 0); // 1, 2, 3, 4
    nad_vec_remove(v, 1); // 1, 3, 4
    nad_vec_remove(v, 2); // 1, 3

    constexpr int32_t want[2] = {1, 3};
    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(v));
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 2);

    nad_vec_drop(v);
}

static void test_remove_of_the_last_elem_empties_the_vec() {
    nad_Vec *v = make_vec(1);

    nad_vec_remove(v, 0);

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(1, nad_vec_cap(v));

    nad_vec_drop(v);
}

// the tail shift is sized in bytes, so wide elems must travel whole
static void test_insert_and_remove_move_whole_elems() {
    const Pair src[2] = {{1, 2}, {3, 4}};

    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_FROM_DATA(Pair, src, 2, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_insert(v, 1, &(Pair){5, 6}));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_INT64(5, NAD_VEC_GET_AS(Pair, v, 1)->a);
    TEST_ASSERT_EQUAL_INT64(6, NAD_VEC_GET_AS(Pair, v, 1)->b);
    TEST_ASSERT_EQUAL_INT64(3, NAD_VEC_GET_AS(Pair, v, 2)->a);
    TEST_ASSERT_EQUAL_INT64(4, NAD_VEC_GET_AS(Pair, v, 2)->b);

    nad_vec_remove(v, 0);

    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(v));
    TEST_ASSERT_EQUAL_INT64(5, NAD_VEC_GET_AS(Pair, v, 0)->a);
    TEST_ASSERT_EQUAL_INT64(6, NAD_VEC_GET_AS(Pair, v, 0)->b);
    TEST_ASSERT_EQUAL_INT64(3, NAD_VEC_GET_AS(Pair, v, 1)->a);
    TEST_ASSERT_EQUAL_INT64(4, NAD_VEC_GET_AS(Pair, v, 1)->b);

    nad_vec_drop(v);
}

/* ========== clear ========== */

static void test_clear_drops_the_len_and_keeps_the_buffer() {
    nad_Vec *v = make_vec(4);
    const void *before = nad_vec_data(v);

    nad_vec_clear(v);

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(4, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));

    // the room is still there, so pushing back needs no allocation
    push_int(v, 7);
    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));
    TEST_ASSERT_EQUAL_INT32(7, *NAD_VEC_GET_AS(int32_t, v, 0));

    nad_vec_drop(v);
}

static void test_clear_of_an_empty_vec_is_a_noop() {
    nad_Vec *v = make_vec(0);

    nad_vec_clear(v);

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(v));
    TEST_ASSERT_NULL(nad_vec_data(v));

    nad_vec_drop(v);
}

/* ========== reserve ========== */

static void test_reserve_grows_the_capacity_and_keeps_the_contents() {
    nad_Vec *v = make_vec(3);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_reserve(v, 16));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(16, nad_vec_cap(v));

    constexpr int32_t want[3] = {0, 1, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 3);

    nad_vec_drop(v);
}

// reserve never shrinks: a request at or below the current capacity changes nothing
static void test_reserve_below_the_capacity_is_a_noop() {
    nad_Vec *v = make_vec_cap(8);
    const void *before = nad_vec_data(v);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_reserve(v, 2));
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_reserve(v, 8));
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_reserve(v, 0));

    TEST_ASSERT_EQUAL_size_t(8, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));

    nad_vec_drop(v);
}

// new_cap * elem_size overflows size_t: reported, never attempted
static void test_reserve_reports_size_overflow() {
    nad_Vec *v = make_vec(3);
    const void *before = nad_vec_data(v);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OUT_OF_MEMORY, nad_vec_reserve(v, SIZE_MAX));

    // the vec is left as it was
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));

    nad_vec_drop(v);
}

/* ========== shrink_to_fit ========== */

static void test_shrink_to_fit_drops_the_slack() {
    nad_Vec *v = make_vec_cap(16);
    push_int(v, 1);
    push_int(v, 2);
    push_int(v, 3);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_shrink_to_fit(v));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_cap(v));

    constexpr int32_t want[3] = {1, 2, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 3);

    nad_vec_drop(v);
}

static void test_shrink_to_fit_when_already_exact_is_a_noop() {
    nad_Vec *v = make_vec(4);
    const void *before = nad_vec_data(v);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_shrink_to_fit(v));

    TEST_ASSERT_EQUAL_size_t(4, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));

    nad_vec_drop(v);
}

// fitting nothing means owning nothing: the buffer is released, not resized to zero
static void test_shrink_to_fit_of_empty_releases_the_buffer() {
    nad_Vec *v = make_vec_cap(8);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_shrink_to_fit(v));

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_cap(v));
    TEST_ASSERT_NULL(nad_vec_data(v));

    // and the vec stays usable afterwards
    push_int(v, 5);
    TEST_ASSERT_EQUAL_INT32(5, *NAD_VEC_GET_AS(int32_t, v, 0));

    nad_vec_drop(v);
}

static void test_shrink_to_fit_of_an_unallocated_vec_is_a_noop() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW(int32_t, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_shrink_to_fit(v));

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_cap(v));
    TEST_ASSERT_NULL(nad_vec_data(v));

    nad_vec_drop(v);
}

/* ========== resize ========== */

static void test_resize_up_zeroes_the_tail() {
    nad_Vec *v = make_vec(2); // 0, 1

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_resize(v, 5));

    constexpr int32_t want[5] = {0, 1, 0, 0, 0};
    TEST_ASSERT_EQUAL_size_t(5, nad_vec_len(v));
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 5);

    nad_vec_drop(v);
}

static void test_resize_down_keeps_the_head_and_the_capacity() {
    nad_Vec *v = make_vec(5);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_resize(v, 2));

    constexpr int32_t want[2] = {0, 1};
    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(5, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 2);

    nad_vec_drop(v);
}

// growing inside the existing room must not move the buffer, only zero the new tail
static void test_resize_within_the_capacity_does_not_reallocate() {
    nad_Vec *v = make_vec_cap(8);
    push_int(v, 1);
    push_int(v, 2);
    const void *before = nad_vec_data(v);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_resize(v, 5));

    constexpr int32_t want[5] = {1, 2, 0, 0, 0};
    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));
    TEST_ASSERT_EQUAL_size_t(8, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 5);

    nad_vec_drop(v);
}

static void test_resize_to_zero_keeps_the_buffer() {
    nad_Vec *v = make_vec(4);
    const void *before = nad_vec_data(v);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_resize(v, 0));

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(4, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));

    nad_vec_drop(v);
}

static void test_resize_to_the_same_len_is_a_noop() {
    nad_Vec *v = make_vec(3);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_resize(v, 3));

    constexpr int32_t want[3] = {0, 1, 2};
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 3);

    nad_vec_drop(v);
}

static void test_resize_reports_size_overflow() {
    nad_Vec *v = make_vec(3);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OUT_OF_MEMORY, nad_vec_resize(v, SIZE_MAX));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_cap(v));

    nad_vec_drop(v);
}

/* ========== swap ========== */

static void test_swap_self_is_noop() {
    nad_Vec *v = make_vec(3);
    const void *before = nad_vec_data(v);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_swap(v, v));

    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_VEC_GET_AS(int32_t, v, 2));

    nad_vec_drop(v);
}

// one allocator on both sides: the buffers are handed over, never copied
static void test_swap_same_allocator_hands_over_buffers() {
    nad_Vec *a = make_vec(2);
    nad_Vec *b = make_vec(5);

    const void *pa = nad_vec_data(a);
    const void *pb = nad_vec_data(b);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_swap(a, b));

    TEST_ASSERT_EQUAL_PTR(pb, nad_vec_data(a));
    TEST_ASSERT_EQUAL_PTR(pa, nad_vec_data(b));
    TEST_ASSERT_EQUAL_size_t(5, nad_vec_len(a));
    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(b));

    nad_vec_drop(b);
    nad_vec_drop(a);
}

// nothing is reallocated, so the spare capacity travels with its buffer
static void test_swap_same_allocator_carries_the_capacity() {
    nad_Vec *a = make_vec_cap(16);
    push_int(a, 1);

    nad_Vec *b = make_vec(3);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_swap(a, b));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(a));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_cap(a));
    TEST_ASSERT_EQUAL_size_t(1, nad_vec_len(b));
    TEST_ASSERT_EQUAL_size_t(16, nad_vec_cap(b));

    nad_vec_drop(b);
    nad_vec_drop(a);
}

// two allocators: buffers cannot be handed over, the bytes are reallocated
static void test_swap_across_allocators_moves_the_bytes() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Vec *a = make_vec(2); // default: 0, 1

    nad_Vec *b = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_OF(int32_t, arena, &b, 10, 20, 30));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_swap(a, b));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(a));
    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(b));
    TEST_ASSERT_EQUAL_INT32(10, *NAD_VEC_GET_AS(int32_t, a, 0));
    TEST_ASSERT_EQUAL_INT32(30, *NAD_VEC_GET_AS(int32_t, a, 2));
    TEST_ASSERT_EQUAL_INT32(0, *NAD_VEC_GET_AS(int32_t, b, 0));
    TEST_ASSERT_EQUAL_INT32(1, *NAD_VEC_GET_AS(int32_t, b, 1));

    // contents move, ownership does not: each vec keeps its own allocator
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_vec_al(a));
    TEST_ASSERT_EQUAL_PTR(arena, nad_vec_al(b));

    nad_vec_drop(b);
    nad_vec_drop(a);
    nad_al_arena_drop(arena);
}

// this path reallocates, so each side comes out fitted to what it received
static void test_swap_across_allocators_fits_each_side_to_its_content() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Vec *a = make_vec_cap(16); // default, len 1 of cap 16
    push_int(a, 7);

    nad_Vec *b = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_OF(int32_t, arena, &b, 1, 2, 3));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_swap(a, b));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(a));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_cap(a));
    TEST_ASSERT_EQUAL_size_t(1, nad_vec_len(b));
    TEST_ASSERT_EQUAL_size_t(1, nad_vec_cap(b));

    nad_vec_drop(b);
    nad_vec_drop(a);
    nad_al_arena_drop(arena);
}

// the empty side allocates nothing and must end up with no buffer at all
static void test_swap_across_allocators_with_an_empty_side() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Vec *a = make_vec_cap(8); // room, but no content

    nad_Vec *b = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_OF(int32_t, arena, &b, 1, 2, 3));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_swap(a, b));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(a));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_VEC_GET_AS(int32_t, a, 1));

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(b));
    TEST_ASSERT_EQUAL_size_t(0, nad_vec_cap(b));
    TEST_ASSERT_NULL(nad_vec_data(b));

    nad_vec_drop(b);
    nad_vec_drop(a);
    nad_al_arena_drop(arena);
}

// The second allocation fails: the first must be given back and both vecs left as they
// were. The order matters for what this can observe — a's side allocates first, through
// the default allocator, so a skipped rollback leaks a real block and LeakSanitizer
// reports it. Run this under ASan or the rollback itself goes unchecked; an arena on that
// side would swallow it, since its dealloc is a no-op.
static void test_swap_across_allocators_rolls_back_a_failed_second_alloc() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Vec *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_OF(int32_t, nad_al_default(), &a, 1, 2));

    nad_Vec *b = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_OF(int32_t, arena, &b, 10, 20, 30));

    const void *pa = nad_vec_data(a);
    const void *pb = nad_vec_data(b);

    // b's side has nothing left to give
    nad_test_arena_leave(arena, 0);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OUT_OF_MEMORY, nad_vec_swap(a, b));

    // both vecs untouched
    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(a));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(b));
    TEST_ASSERT_EQUAL_PTR(pa, nad_vec_data(a));
    TEST_ASSERT_EQUAL_PTR(pb, nad_vec_data(b));
    TEST_ASSERT_EQUAL_INT32(1, *NAD_VEC_GET_AS(int32_t, a, 0));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_VEC_GET_AS(int32_t, a, 1));
    TEST_ASSERT_EQUAL_INT32(30, *NAD_VEC_GET_AS(int32_t, b, 2));

    nad_vec_drop(a);
    nad_al_arena_drop(arena);
}

/* ========== swap_elems ========== */

static void test_swap_elems_exchanges_the_pair() {
    nad_Vec *v = make_vec(4);

    nad_vec_swap_elems(v, 0, 3);

    constexpr int32_t want[4] = {3, 1, 2, 0};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 4);

    nad_vec_drop(v);
}

static void test_swap_elems_same_index_is_noop() {
    nad_Vec *v = make_vec(3);

    nad_vec_swap_elems(v, 1, 1);

    TEST_ASSERT_EQUAL_INT32(1, *NAD_VEC_GET_AS(int32_t, v, 1));

    nad_vec_drop(v);
}

// elem_size drives the swap, so a type wider than a word must move whole
static void test_swap_elems_moves_wide_elems_whole() {
    const Pair src[2] = {{1, 2}, {3, 4}};

    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_FROM_DATA(Pair, src, 2, nad_al_default(), &v));

    nad_vec_swap_elems(v, 0, 1);

    TEST_ASSERT_EQUAL_INT64(3, NAD_VEC_GET_AS(Pair, v, 0)->a);
    TEST_ASSERT_EQUAL_INT64(4, NAD_VEC_GET_AS(Pair, v, 0)->b);
    TEST_ASSERT_EQUAL_INT64(1, NAD_VEC_GET_AS(Pair, v, 1)->a);
    TEST_ASSERT_EQUAL_INT64(2, NAD_VEC_GET_AS(Pair, v, 1)->b);

    nad_vec_drop(v);
}

/* ========== to span ========== */

// the view covers the content, not the allocation
static void test_to_span_follows_the_len_not_the_capacity() {
    nad_Vec *v = make_vec_cap(8);
    push_int(v, 1);
    push_int(v, 2);
    push_int(v, 3);

    const nad_Span s = nad_vec_to_span(v);

    TEST_ASSERT_EQUAL_PTR(nad_vec_data(v), s.data);
    TEST_ASSERT_EQUAL_size_t(3, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);
    TEST_ASSERT_EQUAL_INT32(3, *NAD_SPAN_GET_AS(int32_t, s, 2));

    nad_vec_drop(v);
}

static void test_to_span_mut_writes_reach_the_vec() {
    nad_Vec *v = make_vec(4);

    const nad_SpanMut s = nad_vec_to_span_mut(v);
    NAD_SPAN_SET(int32_t, s, 0, 77);

    TEST_ASSERT_EQUAL_INT32(77, *NAD_VEC_GET_AS(int32_t, v, 0));

    nad_vec_drop(v);
}

static void test_to_span_of_empty_keeps_elem_size() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW(int32_t, nad_al_default(), &v));

    const nad_Span s = nad_vec_to_span(v);

    TEST_ASSERT_NULL(s.data);
    TEST_ASSERT_EQUAL_size_t(0, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);

    nad_vec_drop(v);
}

/* ========== allocation failure ========== */

// len * elem_size overflows size_t: reported, never attempted
static void test_new_len_reports_size_overflow() {
    nad_Vec *v = nullptr;

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OUT_OF_MEMORY,
        nad_vec_new_len(SIZE_MAX, 2, nad_al_default(), &v)
    );

    TEST_ASSERT_NULL(v); // out is untouched on failure
}

static void test_new_cap_reports_size_overflow() {
    nad_Vec *v = nullptr;

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OUT_OF_MEMORY,
        nad_vec_new_cap(SIZE_MAX, 2, nad_al_default(), &v)
    );

    TEST_ASSERT_NULL(v);
}

static void test_from_data_reports_size_overflow() {
    constexpr int32_t src[1] = {1};
    nad_Vec *v = nullptr;

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OUT_OF_MEMORY,
        nad_vec_from_data(src, SIZE_MAX, 2, nad_al_default(), &v)
    );

    TEST_ASSERT_NULL(v);
}

static void test_new_len_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Vec *v = nullptr;

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_VEC_NEW_LEN(int32_t, 1000, arena, &v)
    );

    TEST_ASSERT_NULL(v);

    nad_al_arena_drop(arena);
}

// a push that cannot grow reports it and leaves the vec exactly as it was
static void test_push_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW_CAP(int32_t, 2, arena, &v));

    push_int(v, 1);
    push_int(v, 2);
    const void *before = nad_vec_data(v);

    nad_test_arena_leave(arena, 0);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OUT_OF_MEMORY, NAD_VEC_PUSH(int32_t, v, 3));

    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(2, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));

    constexpr int32_t want[2] = {1, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 2);

    nad_al_arena_drop(arena);
}

// when doubling does not fit, growth falls back to a single extra slot
static void test_push_falls_back_to_one_more_slot_when_doubling_fails() {
    const size_t for_three = arena_charge(3 * sizeof(Pair));
    const size_t for_four = arena_charge(4 * sizeof(Pair));
    if (for_three == for_four) {
        TEST_IGNORE_MESSAGE("the arena's alignment step cannot separate the two requests");
    }

    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW_CAP(Pair, 2, arena, &v));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_push(v, &(Pair){1, 2}));
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_push(v, &(Pair){3, 4}));

    // room for three elems, not for the four that doubling would ask for
    nad_test_arena_leave(arena, for_three);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_vec_push(v, &(Pair){5, 6}));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_cap(v));

    // the elems moved with the buffer
    TEST_ASSERT_EQUAL_INT64(1, NAD_VEC_GET_AS(Pair, v, 0)->a);
    TEST_ASSERT_EQUAL_INT64(3, NAD_VEC_GET_AS(Pair, v, 1)->a);
    TEST_ASSERT_EQUAL_INT64(5, NAD_VEC_GET_AS(Pair, v, 2)->a);
    TEST_ASSERT_EQUAL_INT64(6, NAD_VEC_GET_AS(Pair, v, 2)->b);

    nad_al_arena_drop(arena);
}

/* ========== macros ========== */

static void test_macro_of_builds_from_literals() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_OF(int32_t, nad_al_default(), &v, 4, 5, 6));

    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_vec_elem_size(v));

    constexpr int32_t want[3] = {4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 3);

    nad_vec_drop(v);
}

static void test_macro_of_derives_len_from_the_list() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_OF(int64_t, nad_al_default(), &v, 1, 2, 3, 4, 5));

    TEST_ASSERT_EQUAL_size_t(5, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(sizeof(int64_t), nad_vec_elem_size(v));

    nad_vec_drop(v);
}

static void test_macro_push_appends_a_value() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW(int32_t, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_PUSH(int32_t, v, 4));
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_PUSH(int32_t, v, 5));

    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(v));
    constexpr int32_t want[2] = {4, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 2);

    nad_vec_drop(v);
}

// the value lands in a compound literal, so it must be spelled out exactly once
static void test_macro_push_evaluates_its_value_once() {
    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_NEW(int32_t, nad_al_default(), &v));

    int32_t next = 0;
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_PUSH(int32_t, v, next++));
    }

    TEST_ASSERT_EQUAL_INT32(3, next);
    constexpr int32_t want[3] = {0, 1, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_vec_data(v), 3);

    nad_vec_drop(v);
}

static void test_macro_from_data_infers_elem_size() {
    constexpr int32_t src[2] = {1, 2};

    nad_Vec *v = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_VEC_FROM_DATA(int32_t, src, 2, nad_al_default(), &v));

    TEST_ASSERT_EQUAL_size_t(2, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_vec_elem_size(v));
    TEST_ASSERT_EQUAL_INT32_ARRAY(src, nad_vec_data(v), 2);

    nad_vec_drop(v);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_starts_empty_and_unallocated);
    RUN_TEST(test_new_len_sets_len_cap_and_zeroes);
    RUN_TEST(test_new_cap_reserves_without_length);
    RUN_TEST(test_new_cap_zero_has_no_buffer);
    RUN_TEST(test_from_data_copies_and_detaches_the_source);
    RUN_TEST(test_from_data_empty_has_no_buffer);
    RUN_TEST(test_from_data_copies_whole_elems);
    RUN_TEST(test_from_span_copies_the_view);
    RUN_TEST(test_drop_null_is_noop);

    RUN_TEST(test_copy_is_independent);
    RUN_TEST(test_copy_fits_the_len_not_the_source_capacity);
    RUN_TEST(test_copy_of_empty_has_no_buffer);
    RUN_TEST(test_copy_inherits_the_source_allocator);
    RUN_TEST(test_copy_assign_grows_and_shrinks_the_len);
    RUN_TEST(test_copy_assign_keeps_the_target_capacity);
    RUN_TEST(test_copy_assign_self_is_noop);
    RUN_TEST(test_copy_assign_keeps_the_target_allocator);

    RUN_TEST(test_bytes_is_len_times_elem_size);
    RUN_TEST(test_bytes_ignores_the_spare_capacity);
    RUN_TEST(test_bytes_of_empty_is_zero);
    RUN_TEST(test_bytes_tracks_elem_size);
    RUN_TEST(test_bytes_agrees_with_the_span);

    RUN_TEST(test_set_get_roundtrip);
    RUN_TEST(test_get_mut_writes_through);
    RUN_TEST(test_first_and_last_address_the_ends);
    RUN_TEST(test_first_and_last_mut_write_through);
    RUN_TEST(test_first_and_last_coincide_on_a_single_elem);
    RUN_TEST(test_last_follows_the_len);
    RUN_TEST(test_data_mut_writes_through);

    RUN_TEST(test_push_appends_and_doubles_the_capacity);
    RUN_TEST(test_push_uses_reserved_capacity_without_reallocating);
    RUN_TEST(test_push_moves_whole_elems);
    RUN_TEST(test_pop_shortens_and_keeps_the_capacity);
    RUN_TEST(test_push_after_pop_overwrites_the_slot);

    RUN_TEST(test_insert_at_front_middle_and_end);
    RUN_TEST(test_insert_into_an_empty_vec);
    RUN_TEST(test_insert_grows_when_full);
    RUN_TEST(test_remove_from_front_middle_and_end);
    RUN_TEST(test_remove_of_the_last_elem_empties_the_vec);
    RUN_TEST(test_insert_and_remove_move_whole_elems);

    RUN_TEST(test_clear_drops_the_len_and_keeps_the_buffer);
    RUN_TEST(test_clear_of_an_empty_vec_is_a_noop);

    RUN_TEST(test_reserve_grows_the_capacity_and_keeps_the_contents);
    RUN_TEST(test_reserve_below_the_capacity_is_a_noop);
    RUN_TEST(test_reserve_reports_size_overflow);

    RUN_TEST(test_shrink_to_fit_drops_the_slack);
    RUN_TEST(test_shrink_to_fit_when_already_exact_is_a_noop);
    RUN_TEST(test_shrink_to_fit_of_empty_releases_the_buffer);
    RUN_TEST(test_shrink_to_fit_of_an_unallocated_vec_is_a_noop);

    RUN_TEST(test_resize_up_zeroes_the_tail);
    RUN_TEST(test_resize_down_keeps_the_head_and_the_capacity);
    RUN_TEST(test_resize_within_the_capacity_does_not_reallocate);
    RUN_TEST(test_resize_to_zero_keeps_the_buffer);
    RUN_TEST(test_resize_to_the_same_len_is_a_noop);
    RUN_TEST(test_resize_reports_size_overflow);

    RUN_TEST(test_swap_self_is_noop);
    RUN_TEST(test_swap_same_allocator_hands_over_buffers);
    RUN_TEST(test_swap_same_allocator_carries_the_capacity);
    RUN_TEST(test_swap_across_allocators_moves_the_bytes);
    RUN_TEST(test_swap_across_allocators_fits_each_side_to_its_content);
    RUN_TEST(test_swap_across_allocators_with_an_empty_side);
    RUN_TEST(test_swap_across_allocators_rolls_back_a_failed_second_alloc);

    RUN_TEST(test_swap_elems_exchanges_the_pair);
    RUN_TEST(test_swap_elems_same_index_is_noop);
    RUN_TEST(test_swap_elems_moves_wide_elems_whole);

    RUN_TEST(test_to_span_follows_the_len_not_the_capacity);
    RUN_TEST(test_to_span_mut_writes_reach_the_vec);
    RUN_TEST(test_to_span_of_empty_keeps_elem_size);

    RUN_TEST(test_new_len_reports_size_overflow);
    RUN_TEST(test_new_cap_reports_size_overflow);
    RUN_TEST(test_from_data_reports_size_overflow);
    RUN_TEST(test_new_len_reports_an_exhausted_arena);
    RUN_TEST(test_push_reports_an_exhausted_arena);
    RUN_TEST(test_push_falls_back_to_one_more_slot_when_doubling_fails);

    RUN_TEST(test_macro_of_builds_from_literals);
    RUN_TEST(test_macro_of_derives_len_from_the_list);
    RUN_TEST(test_macro_push_appends_a_value);
    RUN_TEST(test_macro_push_evaluates_its_value_once);
    RUN_TEST(test_macro_from_data_infers_elem_size);

    return UNITY_END();
}
