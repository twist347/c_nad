# nadc — conventions

Naive algorithms & data structures in C23. Single namespace prefix: `nad_` / `NAD_`.

## Naming

**Core rule: one primary type per header, and the type's slug == the module/file name.**
This makes "name by type" and "name by module" the same thing, so there is nothing to
decide per function.

- **Types** are `nad_Pascal`: `nad_Arr`, `nad_Status`, `nad_Allocator`.
  (snake prefix + PascalCase type — avoids the POSIX-reserved `_t` suffix and still
  distinguishes types from values.)
- **Methods** on an owned object: `nad_<slug>_<verb>(<Type> *self, ...)`, receiver first,
  named `self`. The `<slug>` is the lowercase root of the type and equals the module name.
  - `nad_Arr` (module `ds/arr`) → `nad_arr_new`, `nad_arr_drop`, `nad_arr_len`, `nad_arr_get`.
  - `nad_Status` (module `core/status`) → `nad_status_to_str`, `nad_status_safe_set`.
- **Lifetime verbs:** `nad_<slug>_new` / `nad_<slug>_drop`.
- Keep the type suffix and the method slug parallel: `nad_Arr`↔`arr_`. If a type is
  renamed to `nad_Array`, its methods become `nad_array_*`. Never let them drift.

**Exception — interface / strategy types.** A type you *dispatch through* rather than
*own* is named by the operation verb, not `nad_<slug>_<verb>`. The instance is just the
first argument (like `FILE*` in `fprintf`, not `file_printf`).

- `nad_Allocator` is an interface → operations are `nad_alloc`, `nad_calloc`,
  `nad_realloc`, `nad_dealloc` (verb-named, allocator passed as first arg).
- Functions about the allocator *object itself* still use the full slug:
  `nad_allocator_default()`.

Rule of thumb: **owned object → `nad_<slug>_verb(self,...)`; interface you dispatch
through → `nad_verb(iface,...)`.**

**Function-pointer typedefs** are types, so they take the type rule and nothing else:
`nad_Cmp`, `nad_Eq`, `nad_Pred`, `nad_Fold`, `nad_Gen`, `nad_UnOp`, `nad_BinOp`. No `Fn`
suffix and not `_cb` — these are invoked synchronously as part of the operation's own
definition (a comparator is what gives `sort` its meaning), not registered to be called
back later, and a suffix that never distinguishes anything from anything is noise.

**Ready-made values of such a type** are named `nad_<slug>_<T>`, where the slug is the
interface they implement: `nad_cmp_i32`, `nad_cmp_desc_f64`, `nad_eq_cstr`. There is
deliberately no second, value-taking form to tell them apart from — to compare two
values, call the comparator with their addresses.

When the bare name is already taken by a result type, the typedef is named for the agent
instead of the operation: `nad_Hash` is the hash value, so the function that produces one
is `nad_Hasher`.

Function pointers that only ever live inside one interface aggregate stay inline members
with no typedef, as in `nad_Al` and `nad_ElemOps` — a typedef there would name a type that
is never written a second time.

**The `mut` marker.** There is no `const` in any public name: a name either carries `mut`
or it does not, and the unmarked form is the read-only one. Where the marker goes says
what it describes:

- **suffix `_mut`** — the *result* is the mutable one: `nad_arr_get_mut` returns
  `void *` where `nad_arr_get` returns `const void *`, `nad_vec_to_span_mut` returns
  `nad_SpanMut`.
- **after the type slug** — the *argument* is the mutable type while the result is not:
  `nad_span_mut_fprint(nad_SpanMut, ...)`, `nad_span_mut_to_span(nad_SpanMut) ->
  nad_Span`. This only ever comes up for `nad_Span`/`nad_SpanMut`, the one pair that are
  two distinct types; for containers mutability rides on the constness of the receiver
  pointer, so there is nothing to spell.
- **no marker** — there is only one version of the operation, so nothing has to be told
  apart. Every `algo` function taking a `nad_SpanMut` is like this: a constant sort does
  not exist. Those are also not methods on the span — they are free functions over it,
  which is why they carry no receiver marker either.

Rule of thumb: **the marker exists to separate two versions of one operation. Differ by
result → suffix; differ only by argument → after the slug; only one version → nothing.**

**Macros** are `NAD_UPPER`. Type-generic wrappers over functions:
`NAD_ARR_NEW(T, ...)`, `NAD_ALLOC(T, ...)`.

## API contracts

**Error model: uniform status-return.** A fallible operation *returns* `nad_Status`; its
result is written through a trailing `out` pointer. Strictness over ergonomics — the point
is that an error cannot be silently dropped or left un-propagated.

- **Fallible op:** `[[nodiscard]] nad_Status foo(args..., T *out);` — `[[nodiscard]]` makes
  the compiler reject an ignored error; `out` is written **only on `NAD_STATUS_OK`** and
  left untouched on failure. Propagate by hand: `nad_Status st = foo(...); if
  (NAD_STATUS_IS_ERR(st)) return st;` — or `if (...) goto fail;` when resources are held
  (a bare early return would leak them; C has no `defer`).
- **Never** the inverse (`T foo(args, nad_Status *st)`): a status out-param is silently
  ignorable — that reintroduces the "errors are optional" model this project rejected. No
  optional/nullable status params.
- **Can't-fail ops return their value directly**, no status: pure accessors
  (`nad_arr_len`, `nad_arr_elem_size`) — a broken precondition there is a programmer bug,
  see assert vs status. "Uniform" means uniform among *fallible* ops, not literally all.
- **Allocator wrappers are the value-return exception, and it's principled, not a hole.**
  `nad_alloc`/`nad_calloc`/`nad_realloc` return the pointer (`nullptr` = failure): the
  value and the single error cause share one channel, so `[[nodiscard]]` on the pointer
  already enforces the check — wrapping them as `nad_Status f(..., void **out)` would buy
  nothing and fight the `malloc` idiom. The `mem` module has no multi-cause op; the `ds`
  modules do (`get`/`set`/`insert` → OUT_OF_RANGE), so status-return is coherent per module.
- **assert vs status:** `assert` for **programmer errors** (broken preconditions:
  `elem_size > 0`, `alloc != null`, `self != null`, `out != null`) — bugs, not runtime
  states. Return a `nad_Status` for **data-dependent** failures reachable with valid code
  (OOM; `len * elem_size` overflow, folded into `nad_calloc`'s `ckd_mul` → `nullptr` →
  `NAD_STATUS_OUT_OF_MEMORY`). Never turn those into UB or an assert.
- Public symbols are marked `NAD_API` (see `core/export.h`); everything else stays hidden
  under the shared library's default-hidden visibility.

## Build

- C23, CMake ≥ 3.22. `cmake -S . -B build && cmake --build build`.
- Tests (Unity) build when top-level; run the produced `test/**/test_*` binaries or `ctest`.
