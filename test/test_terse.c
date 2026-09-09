#include "terse/ds.h"

#include "support/status.h"

#include <unity.h>

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// the umbrella is one include, and it has to reach every module: a header left out of it
// shows up here as a build error rather than in someone else's project
static void test_the_umbrella_reaches_every_module() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024); // alloc
    TEST_ASSERT_NOT_NULL(arena);

    trs_Vec *v = nullptr;
    TRS_TEST_OK(TRS_VEC_OF(int32_t, arena, &v, 5, 3, 1)); // ds

    trs_span_sort(trs_vec_to_span_mut(v), trs_cmp_i32); // algo, core

    size_t idx;
    TEST_ASSERT_TRUE(trs_span_binary_search(trs_vec_to_span(v), &(int32_t){5}, trs_cmp_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(2, idx);

    TEST_ASSERT_EQUAL_STRING("TRS_STATUS_OK", trs_status_to_str(TRS_STATUS_OK)); // core

    trs_vec_drop(v);
    trs_al_arena_drop(arena);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_the_umbrella_reaches_every_module);

    return UNITY_END();
}
