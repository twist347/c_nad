# nadc — naive algorithms and data structures in C23

[![CI](https://github.com/twist347/c_nad/actions/workflows/ci.yml/badge.svg)](https://github.com/twist347/c_nad/actions/workflows/ci.yml)

Classic containers and algorithms, written plainly. No dependencies.

Two rules shape the whole API:

- **Memory is explicit and swappable.** Nothing allocates on its own — every container is
  handed an `nad_Al *` and uses only that. Swapping in an arena, a pool or a logging
  allocator is a one-line change at the call site.
- **Errors cannot be dropped.** A fallible operation returns `nad_Status` and writes its
  result through a trailing `out`; `[[nodiscard]]` makes ignoring it a compile error.
  Broken preconditions are `assert`, not status — those are bugs, not runtime states.

## What it does not do

- **Elems are bytes.** A container copies `elem_size` bytes in and out, and drops them by
  releasing the block — it never calls anything of yours. A `nad_Vec` of `strdup`ed
  `char *` leaks unless the caller frees them first.
- **Nothing is thread-safe.** No container takes a lock; sharing one across threads is the
  caller's problem.

## Allocators, in three rules

A container is built on one `nad_Al *` and never touches another:

- **A copy is born on its source's allocator** — `nad_vec_copy_with` names another one.
- **An assignment keeps the target's.** On one allocator a move hands the block over and
  cannot fail; across two it costs `n` and may return `NAD_STATUS_ERR_NO_MEM`, leaving
  both sides as they were.
- **`swap` wants both sides on one allocator** — it is O(1) and returns nothing, so a
  mismatch is an `assert`, exactly as C++ leaves it undefined when
  `propagate_on_container_swap` is false. Across two, copy with `copy_with` and hand the
  results over with `move_assign`.

## Example

```c
nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
if (!arena) {
    return 1;
}

nad_Vec *v = nullptr;
if (NAD_STATUS_IS_ERR(NAD_VEC_OF(int32_t, arena, &v, 5, 3, 1, 4, 2))) {
    return 1;
}

nad_span_sort(nad_vec_to_span_mut(v), nad_cmp_i32);

size_t idx;
if (nad_span_binary_search(nad_vec_to_span(v), &(int32_t){4}, nad_cmp_i32, &idx)) {
    printf("4 is at %zu\n", idx); // 4 is at 3
}

nad_al_arena_drop(arena); // the vec went with it
```

## Layout

`nad/nad.h` includes every header below at once; naming the modules a file actually
uses stays the better habit.

**`core`** — the vocabulary the rest is written in.

| | |
|---|---|
| `status.h` | `nad_Status`, what every fallible operation returns |
| `span.h` | `nad_Span` / `nad_SpanMut`, a non-owning view over contiguous elems |
| `cmp.h` | `nad_Cmp` and `nad_Eq`, plus ready-made ones for the built-in types |
| `hash.h` | `nad_Hasher`, `nad_Hash`, hashers for the built-in types and `nad_hash_combine` |
| `print.h` | `nad_FPrint`, the printer a container is handed to show itself, plus ready-made ones for the built-in types |
| `util.h` | `NAD_SWAP`, `NAD_UNUSED`, `NAD_STRINGIFY` |
| `export.h` | `NAD_API` and the visibility it carries |

**`alloc`** — memory, explicit and swappable.

| | |
|---|---|
| `alloc.h` | the `nad_Al` interface and the `nad_alloc` / `nad_calloc` / `nad_realloc` / `nad_dealloc` wrappers |
| `default.h` | malloc and friends |
| `arena.h` | bump allocation, freed all at once |
| `pool.h` | fixed-size blocks off a free list |
| `aligned.h` | wraps another allocator and over-aligns every block it hands out |
| `log.h` | wraps another allocator and writes down what it is asked |

**`algo`** — operations over spans, customized only through function pointers.

| | |
|---|---|
| `fn.h` | `nad_Pred`, `nad_Fold`, `nad_Gen`, `nad_UnOp`, `nad_BinOp` |
| `search.h` | find and its kin, count, the all_of/any_of/none_of trio, min_elem and max_elem, and the binary family over a sorted span |
| `sort.h` | sort and sort_stable, insertion_sort, partial_sort, nth_elem and the is_sorted checks |
| `heap.h` | make_heap, push_heap, pop_heap, sort_heap and the is_heap checks |
| `permute.h` | reverse, rotate, swap_ranges, the partition family and stepping through permutations |
| `modify.h` | remove, remove_if and unique, which return the new length; replace and replace_if, which write in place |
| `copy.h` | copy, copy_if, copy_within |
| `fill.h` | fill, fill_zero, generate |
| `fold.h` | fold, rfold, partial_sum, adjacent_difference |
| `transform.h` | transform and zip |
| `compare.h` | cmp, eq, eq_by, mismatch |
| `merge.h` | merge and inplace_merge |
| `set.h` | union, intersection, difference, symmetric difference and includes, over sorted spans |

**`ds`** — owning containers.

| | |
|---|---|
| `arr.h` | `nad_Arr` — a length fixed at construction |
| `bitset.h` | `nad_BitSet` — a set of indices, one bit each, over a fixed universe |
| `vec.h` | `nad_Vec` — growable, one contiguous block |
| `deque.h` | `nad_Deque` — a ring, both ends O(1) amortized |
| `list.h` | `nad_List` — doubly linked; a position stays valid |
| `hmap.h` | `nad_HMap` — separate chaining; an entry never moves |
| `hset.h` | `nad_HSet` — the same table with nothing on the value side |
| `stack.h` | `nad_Stack` — a vec through a narrower keyhole |
| `queue.h` | `nad_Queue` — a deque through a narrower keyhole |
| `pqueue.h` | `nad_PQueue` — a buffer kept under a heap discipline |

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

Requires a C23 toolchain.

The reference, generated from the same headers:

```sh
doxygen docs/Doxyfile   # -> build-docs/html/index.html
```

## Use it in a project

There is no `install` step yet, so nadc is consumed as a source dependency. Either way the
target to link is the alias `nadc::nadc`, which carries the include path and the C23
requirement with it.

With `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(nadc
    GIT_REPOSITORY https://github.com/twist347/c_nad.git
    GIT_TAG main
)
FetchContent_MakeAvailable(nadc)

target_link_libraries(app PRIVATE nadc::nadc)
```

As a submodule:

```sh
git submodule add https://github.com/twist347/c_nad.git \
    thirdparty/nadc
```

```cmake
add_subdirectory(thirdparty/nadc)

target_link_libraries(app PRIVATE nadc::nadc)
```

## License

MIT — see [LICENSE](https://github.com/twist347/c_nad/blob/main/LICENSE).

`thirdparty/Unity-2.7.0` is Unity, the test framework, vendored as is. It is third-party
code under its own MIT license; its copyright notice lives in
`thirdparty/Unity-2.7.0/LICENSE.txt` and is not covered by the notice above.

`thirdparty/ubench` is ubench.h, the benchmark harness, vendored the same way. There is no
copyright to carry over: it is released into the public domain under the Unlicense, whose
text sits at the top of `thirdparty/ubench/ubench.h`.
