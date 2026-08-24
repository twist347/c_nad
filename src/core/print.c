#include "nad/core/print.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

/* ========== internals ========== */

// One body per type, differing only in the cast and the conversion. The format stays a
// literal at the call site, so the compiler still checks it against the argument.
#define DEFINE_FPRINT(name, T, fmt)                         \
    void nad_fprint_##name(FILE *stream, const void *val) { \
        assert(stream);                                     \
        assert(val);                                        \
        fprintf(stream, fmt, *(const T *) val);             \
    }

/* ========== instantiation ========== */

DEFINE_FPRINT(i8, int8_t, "%" PRId8)
DEFINE_FPRINT(i16, int16_t, "%" PRId16)
DEFINE_FPRINT(i32, int32_t, "%" PRId32)
DEFINE_FPRINT(i64, int64_t, "%" PRId64)

DEFINE_FPRINT(u8, uint8_t, "%" PRIu8)
DEFINE_FPRINT(u16, uint16_t, "%" PRIu16)
DEFINE_FPRINT(u32, uint32_t, "%" PRIu32)
DEFINE_FPRINT(u64, uint64_t, "%" PRIu64)

DEFINE_FPRINT(size, size_t, "%zu")
DEFINE_FPRINT(ptrdiff, ptrdiff_t, "%td")

// float promotes to double on the way into fprintf, which is what %g expects
DEFINE_FPRINT(f32, float, "%g")
DEFINE_FPRINT(f64, double, "%g")

/* ========== bool ========== */

// the one entry core/cmp and core/hash do without: for them u8 carries a bool whole, but
// the readable form of a bool is a word, not a digit
void nad_fprint_bool(FILE *stream, const void *val) {
    assert(stream);
    assert(val);

    fputs(*(const bool *) val ? "true" : "false", stream);
}

/* ========== char ========== */

void nad_fprint_char(FILE *stream, const void *val) {
    assert(stream);
    assert(val);

    const char c = *(const char *) val;

    // isprint takes an int that has to be representable as unsigned char, and char may be
    // signed: the cast is what keeps a negative one out of undefined behaviour
    if (isprint((unsigned char) c)) {
        fputc(c, stream);
    } else {
        fprintf(stream, "\\x%02x", (unsigned) (unsigned char) c);
    }
}

/* ========== cstr ========== */

void nad_fprint_cstr(FILE *stream, const void *val) {
    assert(stream);
    assert(val);

    // the operand is a pointer to a pointer, as in nad_cmp_cstr and nad_hash_cstr
    const char *str = *(const char *const *) val;

    if (!str) {
        fputs("null", stream);
        return;
    }

    fprintf(stream, "\"%s\"", str);
}
