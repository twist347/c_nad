#include "nad/algo/sort.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

static int cmp_i32(const void *a, const void *b) {
    const int32_t x = *(const int32_t *) a;
    const int32_t y = *(const int32_t *) b;
    return (x > y) - (x < y);
}

static int cmp_i32_desc(const void *a, const void *b) {
    return cmp_i32(b, a);
}

// ordered by key only, so tag can witness whether equal elements kept their order
typedef struct {
    int32_t key;
    int32_t tag;
} Tagged;

static int cmp_tagged(const void *a, const void *b) {
    const int32_t x = ((const Tagged *) a)->key;
    const int32_t y = ((const Tagged *) b)->key;
    return (x > y) - (x < y);
}

/* ========== insertion_sort ========== */

static void test_sort_orders_a_shuffled_span() {
    int32_t buf[6] = {5, 3, 1, 4, 2, 6};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    nad_span_insertion_sort(s, cmp_i32);

    constexpr int32_t expected[6] = {1, 2, 3, 4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 6);
}

static void test_sort_already_sorted_is_unchanged() {
    int32_t buf[4] = {1, 2, 3, 4};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 4);

    nad_span_insertion_sort(s, cmp_i32);

    constexpr int32_t expected[4] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 4);
}

// the worst case for insertion sort — every element travels the whole way
static void test_sort_reversed_span() {
    int32_t buf[5] = {5, 4, 3, 2, 1};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_insertion_sort(s, cmp_i32);

    constexpr int32_t expected[5] = {1, 2, 3, 4, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 5);
}

static void test_sort_keeps_duplicates() {
    int32_t buf[6] = {3, 1, 3, 2, 1, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    nad_span_insertion_sort(s, cmp_i32);

    constexpr int32_t expected[6] = {1, 1, 2, 3, 3, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 6);
}

static void test_sort_empty_and_single_are_noop() {
    int32_t buf[1] = {42};

    nad_span_insertion_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 0), cmp_i32);
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);

    nad_span_insertion_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 1), cmp_i32);
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);
}

// the comparator defines the order — the algorithm must not assume ascending
static void test_sort_follows_the_comparator() {
    int32_t buf[5] = {2, 5, 1, 4, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_insertion_sort(s, cmp_i32_desc);

    constexpr int32_t expected[5] = {5, 4, 3, 2, 1};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 5);
}

// stability: equal keys must come out in their original relative order
static void test_sort_is_stable() {
    Tagged buf[5] = {
        {2, 0}, {1, 0}, {2, 1}, {1, 1}, {2, 2},
    };
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(Tagged, buf, 5);

    nad_span_insertion_sort(s, cmp_tagged);

    TEST_ASSERT_EQUAL_INT32(1, buf[0].key);
    TEST_ASSERT_EQUAL_INT32(0, buf[0].tag);
    TEST_ASSERT_EQUAL_INT32(1, buf[1].key);
    TEST_ASSERT_EQUAL_INT32(1, buf[1].tag);
    TEST_ASSERT_EQUAL_INT32(2, buf[2].key);
    TEST_ASSERT_EQUAL_INT32(0, buf[2].tag);
    TEST_ASSERT_EQUAL_INT32(2, buf[3].key);
    TEST_ASSERT_EQUAL_INT32(1, buf[3].tag);
    TEST_ASSERT_EQUAL_INT32(2, buf[4].key);
    TEST_ASSERT_EQUAL_INT32(2, buf[4].tag);
}

static void test_sort_stays_within_the_subspan() {
    int32_t buf[5] = {9, 3, 1, 2, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_insertion_sort(nad_span_sub_mut(s, 1, 3), cmp_i32);

    constexpr int32_t expected[5] = {9, 1, 2, 3, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 5);
}

/* ========== is_sorted ========== */

static void test_is_sorted_accepts_ascending() {
    constexpr int32_t buf[4] = {1, 2, 3, 4};

    TEST_ASSERT_TRUE(nad_span_is_sorted(NAD_SPAN_NEW(int32_t, buf, 4), cmp_i32));
}

static void test_is_sorted_allows_equal_neighbours() {
    constexpr int32_t buf[4] = {1, 2, 2, 3};

    TEST_ASSERT_TRUE(nad_span_is_sorted(NAD_SPAN_NEW(int32_t, buf, 4), cmp_i32));
}

// the break of order is in the last pair — the walk must reach it
static void test_is_sorted_rejects_a_late_inversion() {
    constexpr int32_t buf[4] = {1, 2, 3, 0};

    TEST_ASSERT_FALSE(nad_span_is_sorted(NAD_SPAN_NEW(int32_t, buf, 4), cmp_i32));
}

static void test_is_sorted_empty_and_single_are_sorted() {
    constexpr int32_t buf[1] = {42};

    TEST_ASSERT_TRUE(nad_span_is_sorted(NAD_SPAN_NEW(int32_t, buf, 0), cmp_i32));
    TEST_ASSERT_TRUE(nad_span_is_sorted(NAD_SPAN_NEW(int32_t, buf, 1), cmp_i32));
}

static void test_is_sorted_agrees_with_sort() {
    int32_t buf[6] = {4, 1, 6, 2, 5, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    TEST_ASSERT_FALSE(nad_span_is_sorted(nad_span_from_mut(s), cmp_i32));
    nad_span_insertion_sort(s, cmp_i32);
    TEST_ASSERT_TRUE(nad_span_is_sorted(nad_span_from_mut(s), cmp_i32));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_sort_orders_a_shuffled_span);
    RUN_TEST(test_sort_already_sorted_is_unchanged);
    RUN_TEST(test_sort_reversed_span);
    RUN_TEST(test_sort_keeps_duplicates);
    RUN_TEST(test_sort_empty_and_single_are_noop);
    RUN_TEST(test_sort_follows_the_comparator);
    RUN_TEST(test_sort_is_stable);
    RUN_TEST(test_sort_stays_within_the_subspan);

    RUN_TEST(test_is_sorted_accepts_ascending);
    RUN_TEST(test_is_sorted_allows_equal_neighbours);
    RUN_TEST(test_is_sorted_rejects_a_late_inversion);
    RUN_TEST(test_is_sorted_empty_and_single_are_sorted);
    RUN_TEST(test_is_sorted_agrees_with_sort);

    return UNITY_END();
}
