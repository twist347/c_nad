#pragma once

/// comparator: <0 if a<b, 0 if a==b, >0 if a>b. qsort compatible
typedef int (*nad_cmp_fn)(const void *, const void *);

typedef int (*nad_cmp_ctx_fn)(const void *, const void *, void *);

/// equality: true if a == b.
typedef bool (*nad_eq_fn)(const void *, const void *);
