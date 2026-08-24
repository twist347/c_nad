// for @snippet

#include "nad/alloc/default.h"
#include "nad/alloc/log.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    /// [wrap]
    // allocators compose: the log passes everything through and writes a line about it,
    // and its own two blocks come from the allocator it wraps
    nad_Al *log = nad_al_log_new(nad_al_default(), stdout);
    if (!log) {
        return 1;
    }

    int32_t *xs = NAD_CALLOC(int32_t, log, 4); // [NAD] calloc num = 4 size = 4 -> 0x...
    if (xs) {
        NAD_DEALLOC(int32_t, log, xs, 4);      // [NAD] dealloc 0x... size = 16
    }

    // the wrapped allocator outlives the log, which is what lets the drop give the two
    // blocks back to it
    nad_al_log_drop(log);
    /// [wrap]

    return xs ? 0 : 1;
}
