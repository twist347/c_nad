#pragma once

#include "nad/core/status.h"

#include "unity.h"

#include <stdio.h>

/* ========== macros ========== */

#define NAD_TEST_STATUS(want, expr) \
    NAD_TEST_STATUS_((want), (expr), #expr)

#define NAD_TEST_OK(expr) \
    NAD_TEST_STATUS_(NAD_STATUS_OK, (expr), #expr)

/* ========== internals ========== */

// 'text' is stringified by the caller, so the message shows what the test
// wrote, not what the preprocessor made of it
#define NAD_TEST_STATUS_(want, expr, text)                                          \
    do {                                                                            \
        const nad_Status nad_test_got_ = (expr);                                    \
        if (nad_test_got_ != (want)) {                                              \
            TEST_FAIL_MESSAGE(nad_test_status_msg_((text), (want), nad_test_got_)); \
        }                                                                           \
    } while (0)

[[nodiscard]]
static inline const char *nad_test_status_msg_(const char *text, nad_Status want, nad_Status got) {
    static char buf[256];
    snprintf(
        buf, sizeof buf, "%s: expected %s, got %s",
        text, nad_status_to_str(want), nad_status_to_str(got)
    );

    return buf;
}
