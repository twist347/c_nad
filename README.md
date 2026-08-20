# nadc — naive algorithms and data structures in C23

[![CI](https://github.com/twist347/c-naive-algorithms-and-data-structures/actions/workflows/ci.yml/badge.svg)](https://github.com/twist347/c-naive-algorithms-and-data-structures/actions/workflows/ci.yml)

Classic containers and algorithms, written plainly. No dependencies.

Two rules shape the whole API:

- **Memory is explicit and swappable.** Nothing allocates on its own — every container is
  handed an `nad_Al *` and uses only that. Swapping in an arena, a pool or a logging
  allocator is a one-line change at the call site.
- **Errors cannot be dropped.** A fallible operation returns `nad_Status` and writes its
  result through a trailing `out`; `[[nodiscard]]` makes ignoring it a compile error.
  Broken preconditions are `assert`, not status — those are bugs, not runtime states.

```c
nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);

nad_Vec *v = nullptr;
if (NAD_STATUS_IS_ERR(NAD_VEC_OF(int32_t, arena, &v, 5, 3, 1, 4, 2))) {
    return 1;
}

nad_span_sort(nad_vec_to_span_mut(v), nad_cmp_i32);

size_t idx;
if (nad_span_bsearch(nad_vec_to_span(v), &(int32_t){4}, nad_cmp_i32, &idx)) {
    printf("4 is at %zu\n", idx); // 4 is at 3
}

nad_al_arena_drop(arena); // the vec went with it
```

## Layout

| | |
|---|---|
| `core` | `nad_Status`, `nad_Span` (non-owning view), comparators and hashers for the built-in types, `nad_ElemOps` |
| `alloc` | the `nad_Al` interface + default, arena, pool and logging allocators |
| `ds` | `nad_Arr` (fixed), `nad_Vec` (growable), `nad_List` (doubly linked) |
| `algo` | 52 operations over spans: sort, search, compare, copy, fill, fold, merge, modify, permute, transform — customized only through function pointers: `nad_Cmp`, `nad_Eq`, `nad_Pred` and the fold, generate and transform forms in `algo/fn.h` |

Dependencies run one way — `core` <- `alloc` <- `algo` <- `ds`. A span carries no
allocator and owns nothing, so it sits in `core` next to the other vocabulary types;
that is what lets a container reach for an algorithm without the layers looping back.

## Build

```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build
```

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all"
cmake --build build-asan && ctest --test-dir build-asan
```

Requires a C23 toolchain
