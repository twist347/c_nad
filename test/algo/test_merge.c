#include "nad/algo/merge.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// ordered by key only, so tag can witness which side an equal element came from
typedef struct {
    int32_t key;
    int32_t tag;
} Tagged;

static int cmp_tagged(const void *a, const void *b) {
    return nad_cmp_i32(&((const Tagged *) a)->key, &((const Tagged *) b)->key);
}

/* ========== merge ========== */

static void test_merge_interleaves_both_sides() {
    constexpr int32_t a[3] = {1, 3, 5};
    constexpr int32_t b[3] = {2, 4, 6};
    int32_t dst[6] = {0};

    nad_span_merge(
        NAD_SPAN_NEW_MUT(int32_t, dst, 6),
        NAD_SPAN_NEW(int32_t, a, 3),
        NAD_SPAN_NEW(int32_t, b, 3),
        nad_cmp_i32
    );

    constexpr int32_t expected[6] = {1, 2, 3, 4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, dst, 6);
}

// no interleaving at all — the whole tail must still be drained
static void test_merge_disjoint_ranges() {
    constexpr int32_t a[3] = {1, 2, 3};
    constexpr int32_t b[3] = {7, 8, 9};
    int32_t dst[6] = {0};

    nad_span_merge(
        NAD_SPAN_NEW_MUT(int32_t, dst, 6),
        NAD_SPAN_NEW(int32_t, a, 3),
        NAD_SPAN_NEW(int32_t, b, 3),
        nad_cmp_i32
    );

    constexpr int32_t expected[6] = {1, 2, 3, 7, 8, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, dst, 6);
}

static void test_merge_second_side_comes_first() {
    constexpr int32_t a[2] = {7, 8};
    constexpr int32_t b[2] = {1, 2};
    int32_t dst[4] = {0};

    nad_span_merge(
        NAD_SPAN_NEW_MUT(int32_t, dst, 4),
        NAD_SPAN_NEW(int32_t, a, 2),
        NAD_SPAN_NEW(int32_t, b, 2),
        nad_cmp_i32
    );

    constexpr int32_t expected[4] = {1, 2, 7, 8};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, dst, 4);
}

static void test_merge_uneven_lengths() {
    constexpr int32_t a[1] = {4};
    constexpr int32_t b[5] = {1, 2, 3, 5, 6};
    int32_t dst[6] = {0};

    nad_span_merge(
        NAD_SPAN_NEW_MUT(int32_t, dst, 6),
        NAD_SPAN_NEW(int32_t, a, 1),
        NAD_SPAN_NEW(int32_t, b, 5),
        nad_cmp_i32
    );

    constexpr int32_t expected[6] = {1, 2, 3, 4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, dst, 6);
}

static void test_merge_with_an_empty_side() {
    constexpr int32_t a[3] = {1, 2, 3};
    int32_t dst[3] = {0};

    nad_span_merge(
        NAD_SPAN_NEW_MUT(int32_t, dst, 3),
        NAD_SPAN_NEW(int32_t, a, 3),
        NAD_SPAN_NEW(int32_t, a, 0),
        nad_cmp_i32
    );
    TEST_ASSERT_EQUAL_INT32_ARRAY(a, dst, 3);

    // and the mirror case — the empty side leading
    int32_t dst2[3] = {0};
    nad_span_merge(
        NAD_SPAN_NEW_MUT(int32_t, dst2, 3),
        NAD_SPAN_NEW(int32_t, a, 0),
        NAD_SPAN_NEW(int32_t, a, 3),
        nad_cmp_i32
    );
    TEST_ASSERT_EQUAL_INT32_ARRAY(a, dst2, 3);
}

static void test_merge_both_sides_empty() {
    constexpr int32_t a[1] = {1};
    int32_t dst[1] = {42};

    nad_span_merge(
        NAD_SPAN_NEW_MUT(int32_t, dst, 0),
        NAD_SPAN_NEW(int32_t, a, 0),
        NAD_SPAN_NEW(int32_t, a, 0),
        nad_cmp_i32
    );

    TEST_ASSERT_EQUAL_INT32(42, dst[0]);
}

static void test_merge_keeps_duplicates() {
    constexpr int32_t a[3] = {1, 2, 2};
    constexpr int32_t b[2] = {2, 3};
    int32_t dst[5] = {0};

    nad_span_merge(
        NAD_SPAN_NEW_MUT(int32_t, dst, 5),
        NAD_SPAN_NEW(int32_t, a, 3),
        NAD_SPAN_NEW(int32_t, b, 2),
        nad_cmp_i32
    );

    constexpr int32_t expected[5] = {1, 2, 2, 2, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, dst, 5);
}

// stability: on a tie the element from the first span must be emitted first
static void test_merge_is_stable_on_ties() {
    constexpr Tagged a[2] = {{1, 100}, {2, 101}};
    constexpr Tagged b[2] = {{1, 200}, {2, 201}};
    Tagged dst[4] = {};

    nad_span_merge(
        NAD_SPAN_NEW_MUT(Tagged, dst, 4),
        NAD_SPAN_NEW(Tagged, a, 2),
        NAD_SPAN_NEW(Tagged, b, 2),
        cmp_tagged
    );

    TEST_ASSERT_EQUAL_INT32(1, dst[0].key);
    TEST_ASSERT_EQUAL_INT32(100, dst[0].tag);
    TEST_ASSERT_EQUAL_INT32(1, dst[1].key);
    TEST_ASSERT_EQUAL_INT32(200, dst[1].tag);
    TEST_ASSERT_EQUAL_INT32(2, dst[2].key);
    TEST_ASSERT_EQUAL_INT32(101, dst[2].tag);
    TEST_ASSERT_EQUAL_INT32(2, dst[3].key);
    TEST_ASSERT_EQUAL_INT32(201, dst[3].tag);
}

static void test_merge_writes_only_into_the_destination_view() {
    constexpr int32_t a[1] = {1};
    constexpr int32_t b[1] = {2};
    int32_t dst[4] = {9, 0, 0, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, dst, 4);

    nad_span_merge(
        nad_span_sub_mut(s, 1, 2),
        NAD_SPAN_NEW(int32_t, a, 1),
        NAD_SPAN_NEW(int32_t, b, 1),
        nad_cmp_i32
    );

    constexpr int32_t expected[4] = {9, 1, 2, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, dst, 4);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_merge_interleaves_both_sides);
    RUN_TEST(test_merge_disjoint_ranges);
    RUN_TEST(test_merge_second_side_comes_first);
    RUN_TEST(test_merge_uneven_lengths);

    RUN_TEST(test_merge_with_an_empty_side);
    RUN_TEST(test_merge_both_sides_empty);

    RUN_TEST(test_merge_keeps_duplicates);
    RUN_TEST(test_merge_is_stable_on_ties);
    RUN_TEST(test_merge_writes_only_into_the_destination_view);

    return UNITY_END();
}
