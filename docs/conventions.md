# trs — conventions

The rules the library is written to, and why each is what it is. C23, one namespace
prefix: `trs_` / `TRS_`.

## Naming

**Core rule: one primary type per header, and the type's slug == the module name.** That
makes "name by type" and "name by module" the same thing, so nothing is decided per
function.

- **Types** are `trs_Pascal`: `trs_Arr`, `trs_Status`, `trs_Al`. Snake prefix plus
  PascalCase avoids the POSIX-reserved `_t` and still separates types from values.
- **Methods** on an owned object: `trs_<slug>_<verb>(<Type> *self, ...)`, receiver first,
  named `self`. `trs_Arr` (module `ds/arr`) → `trs_arr_new_len`, `trs_arr_drop`,
  `trs_arr_len`, `trs_arr_get`.
- **Lifetime verbs** are `new` and `drop`: `trs_vec_new` / `trs_vec_drop`. A type that
  cannot be built empty spells out what it needs — `trs_arr_new_len`, because an array's
  length is fixed at construction.
- Type suffix and method slug stay parallel: `trs_Arr` ↔ `arr_`. Rename the type to
  `trs_Array` and the methods become `trs_array_*`. Never let them drift.

**Exception — interfaces.** A type you *dispatch through* rather than *own* is named by
the operation verb; the instance is just the first argument, like `FILE *` in `fprintf`
and not `file_printf`.

- `trs_Al` is an interface → `trs_alloc`, `trs_calloc`, `trs_realloc`, `trs_dealloc`.
- Functions about the allocator *object* keep the full slug: `trs_al_default`.

Rule of thumb: **owned object → `trs_<slug>_verb(self, ...)`; interface →
`trs_verb(iface, ...)`.**

**Function-pointer typedefs** are types, so they follow the type rule and nothing else:
`trs_Cmp`, `trs_Eq`, `trs_Pred`, `trs_Fold`, `trs_Gen`, `trs_UnOp`, `trs_BinOp`. No `Fn`
suffix, no `_cb`: they are called synchronously as part of the operation's own definition
— a comparator is what gives `sort` its meaning — not registered to fire later, and a
suffix that distinguishes nothing is noise.

**Ready-made values** of such a type are `trs_<slug>_<T>`, the slug being the interface
they implement: `trs_cmp_i32`, `trs_cmp_desc_f64`, `trs_eq_cstr`. There is deliberately no
second, value-taking form to tell them apart from: to compare two values, call the
comparator with their addresses.

Where the bare name is already a result type, the typedef is named for the agent rather
than the operation — `trs_Hash` is the value, so `trs_Hasher` produces one.

A function pointer that only ever lives inside one interface aggregate stays an inline
member with no typedef, as in `trs_Al`: a typedef would name a type never written twice.

**The `mut` marker.** No public name carries `const` — a name either says `mut` or it does
not, and the unmarked form is the read-only one. Where the marker sits says what it
describes:

- **suffix `_mut`** — the *result* is mutable. `trs_arr_get_mut` returns `void *` where
  `trs_arr_get` returns `const void *`; `trs_vec_to_span_mut` returns `trs_SpanMut`.
- **after the type slug** — the *argument* is the mutable type, the result is not:
  `trs_span_mut_to_span(trs_SpanMut) -> trs_Span`, `trs_span_mut_fprint(trs_SpanMut, …)`.
  Only `trs_Span`/`trs_SpanMut` need it, being the one pair that are two distinct types;
  elsewhere mutability rides on the constness of the receiver pointer.
- **no marker** — there is one version, so nothing has to be told apart. Every `algo`
  function over a `trs_SpanMut` is like this: a constant sort does not exist.

Rule of thumb: **the marker separates two versions of one operation. Differ by result →
suffix; differ only by argument → after the slug; one version → nothing.**

**Parameter names go by role.**

- `self` — the receiver of a method on an owned object. A `trs_Span` is `self` in its own
  ops and `s` in `algo`, which are free functions over a span, not methods on it.
- `obj` — the object under construction, from allocation until its invariant holds; `self`
  is only ever the one that arrived ready. Where it has a truer job it takes that name
  instead: `copy`, `clone`.
- `key` what is looked up or indexed by, `val` any other value passed by address, `data`
  raw elems to copy in, `ptr` a block from an allocator. No `x`: a value has a job, not a
  letter.
- `lhs`/`rhs` two elems being compared; `a`/`b` two of anything larger — two spans, two
  hashes.
- `idx` one index, `i`/`j` a pair of them, `at` a node position.
- `len` how many elems there are, `cap` how many fit, `count` how many an argument asks
  for, `nth` an ordinal. `num` only in `trs_calloc`, where `malloc`'s spelling wins.

**Macros** are `TRS_UPPER`, but the prefix is for what leaves the translation unit. A
`static` constant or a macro that lives and dies inside one `.c` shares no namespace with
anyone and goes unprefixed: `VEC_GROWTH_BASE`, `INSERTION_THRESHOLD`, `FNV_PRIME_64`.
Names in `src/internal/*.h` keep it, since several `.c` see them at once, and so do the
type-generic wrappers over functions: `TRS_ARR_NEW_LEN(T, …)`, `TRS_ALLOC(T, …)`.

## API contracts

**Error model: uniform status-return.** A fallible operation *returns* `trs_Status` and
writes its result through a trailing `out`. Strictness over ergonomics — the point is that
an error cannot be silently dropped.

- **Fallible op:** `[[nodiscard]] trs_Status foo(args…, T *out);`. `[[nodiscard]]` turns an
  ignored error into a diagnostic the compiler raises without being asked, and into a hard
  error under `-Werror`; `out` is written **only on `TRS_STATUS_OK`**, left untouched
  otherwise. Propagate by hand — `trs_Status st = foo(…); if (TRS_STATUS_IS_ERR(st))
  return st;` — or `goto fail` while resources are held, since C has no `defer` and a bare
  early return would leak them.
- **Never the inverse** (`T foo(args, trs_Status *st)`): a status out-param is silently
  ignorable, which is the "errors are optional" model this library rejects.
- **Can't-fail ops return their value directly**, with no status: pure accessors such as
  `trs_arr_len` and `trs_arr_elem_size`. Uniform means uniform among *fallible* ops.
- **Allocator wrappers are the value-return exception, and a principled one.** `trs_alloc`,
  `trs_calloc` and `trs_realloc` return the pointer, `nullptr` meaning failure: the value
  and the single error cause share one channel, so `[[nodiscard]]` already flags a dropped
  check. `trs_Status f(…, void **out)` would buy nothing and fight the `malloc` idiom.
- **Everything else keeps the status**, though OOM is nearly its only cause, because the
  status is a type-level marker that an op *can* fail rather than a carrier of causes. The
  `malloc` idiom does not generalize: most fallible ops have nothing to hand back, and
  `bool` is spoken for — it returns *found* from `contains`, `is_empty` and
  `binary_search`, while `hmap` and `hset` `insert` need both meanings at once. A second
  cause is then one more enumerator, not a new signature everywhere.
- **assert vs status.** `assert` is for programmer errors — broken preconditions like
  `elem_size > 0`, `al != nullptr`, `self != nullptr`, `out != nullptr`. Those are bugs,
  not runtime states. `trs_Status` is for data-dependent failures reachable from correct
  code: OOM, and `len * elem_size` overflow folded into `trs_calloc`'s `ckd_mul` →
  `nullptr` → `TRS_STATUS_ERR_NO_MEM`. Never turn one of those into UB or an assert.
- Public symbols carry `TRS_API` (see `core/export.h`); everything else stays hidden under
  the library's default-hidden visibility.
