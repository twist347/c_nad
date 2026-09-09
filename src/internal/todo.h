#pragma once

#include <stdio.h>
#include <stdlib.h>

[[noreturn]]
static inline void trs_not_implemented_(const char *file, int line, const char *func) {
    fprintf(stderr, "%s:%d: %s: not implemented", file, line, func);
    abort();
}

static inline void trs_unused_(int dummy, ...) { (void) dummy; }

#define TRS_NOT_IMPLEMENTED(...) \
    (trs_unused_(0 __VA_OPT__(,) __VA_ARGS__), trs_not_implemented_(__FILE__, __LINE__, __func__))
