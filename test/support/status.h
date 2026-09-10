#pragma once

#include "trs/core/status.h"

#include <unity.h>

#include <stdio.h>

/* ========== macros ========== */

#define TRS_TEST_STATUS(want, expr) \
    TRS_TEST_STATUS_((want), (expr), #expr)

#define TRS_TEST_OK(expr) \
    TRS_TEST_STATUS_(TRS_STATUS_OK, (expr), #expr)

/* ========== internals ========== */

// 'text' is stringified by the caller, so the message shows what the test
// wrote, not what the preprocessor made of it
#define TRS_TEST_STATUS_(want, expr, text)                                          \
    do {                                                                            \
        const trs_Status trs_test_got_ = (expr);                                    \
        if (trs_test_got_ != (want)) {                                              \
            TEST_FAIL_MESSAGE(trs_test_status_msg_((text), (want), trs_test_got_)); \
        }                                                                           \
    } while (0)

[[nodiscard]]
static inline const char *trs_test_status_msg_(const char *text, trs_Status want, trs_Status got) {
    static char buf[256];
    snprintf(buf, sizeof buf, "%s: expected %s, got %s", text, trs_status_to_str(want), trs_status_to_str(got));

    return buf;
}
