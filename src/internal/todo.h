#pragma once

#include <stdio.h>
#include <stdlib.h>

[[noreturn]]
static inline void tda_not_implemented_(const char *file, int line, const char *func) {
    fprintf(stderr, "%s:%d: %s: not implemented", file, line, func);
    abort();
}

static inline void tda_unused_(int dummy, ...) { (void) dummy; }

#define TDA_NOT_IMPLEMENTED(...) \
    (tda_unused_(0 __VA_OPT__(,) __VA_ARGS__), tda_not_implemented_(__FILE__, __LINE__, __func__))
