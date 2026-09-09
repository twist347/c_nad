// for @snippet

#include "tda/alloc/default.h"
#include "tda/alloc/log.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    /// [wrap]
    // allocators compose: the log passes everything through and writes a line about it,
    // and its own two blocks come from the allocator it wraps
    tda_Al *log = tda_al_log_new(tda_al_default(), stdout);
    if (!log) {
        return 1;
    }

    int32_t *xs = TDA_CALLOC(int32_t, log, 4); // [TDA] calloc num = 4 size = 4 -> 0x...
    if (xs) {
        TDA_DEALLOC(int32_t, log, xs, 4);      // [TDA] dealloc 0x... size = 16
    }

    // the wrapped allocator outlives the log, which is what lets the drop give the two
    // blocks back to it
    tda_al_log_drop(log);
    /// [wrap]

    return xs ? 0 : 1;
}
