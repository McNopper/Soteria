# Soteria — MISRA C++:2023 Deviation Register

> Every deviation from MISRA C++:2023 is recorded here and annotated at the
> point of use with `MISRA_DEVIATION("Rule X.Y.Z", "…")` plus a matching
> `// NOLINT(check-name)` comment (see `engine/core/safety_macros.hpp`).
> Format follows the deviation-appendix pattern used by the Vulkan SC
> specification itself for `vulkan_sc_core.h`.
>
> Rule numbers use MISRA C++:2023 (October 2023) numbering and were verified
> against the published document. Rule summaries below are *paraphrases* —
> the guideline text is © The MISRA Consortium Limited and is not
> redistributed with this project; the licensed document lives gitignored
> under `references/`. "MISRA" is a registered trademark of The MISRA
> Consortium Limited; no endorsement is implied.

## Register

| ID | Rule | Category | Location | Summary |
|----|------|----------|----------|---------|
| DEV-001 | 8.2.6 | Required | `engine/rendering/vertex_buffer.cpp` (`VertexBuffer::Write`) | `void*` → `uint8_t*` cast for mapped Vulkan SC memory |
| DEV-002 | 30.0.1 | Required | `engine/core/log.cpp` (`log::Write`) | C stdio (`fputs`) for debug logging |

---

### DEV-001 — Rule 8.2.6: cast from `void*` to `uint8_t*`

**Rule (Required), paraphrased:** objects of integral, enumerated or
pointer-to-void type must not be converted to pointer type.

**Location:** `engine/rendering/vertex_buffer.cpp`, `VertexBuffer::Write()`.

**Rationale:** `vkMapMemory` returns `void*` by API design; writing vertex
data into the mapped region requires a typed pointer. The Vulkan SC API
offers no alternative signature.

**Risk mitigation:**
- The cast is confined to this single function; callers use the bounds-
  checked `Write()` and never touch the raw pointer.
- Buffer size is validated before the copy (`bytes <= m_sizeBytes`), so the
  cast cannot produce an out-of-bounds access (Rule 8.7.1 preserved).
- The alternative escape hatch (`MappedPtr()`) is documented as requiring
  its own deviation at the call site.

**Verification:** unit tests exercise `Write()` bounds checks; clang-tidy
`cppcoreguidelines-pro-type-reinterpret-cast` (Rule 8.2.5) remains fully
enforced — only `static_cast` from `void*` is used, which Rule 8.2.6 covers.

---

### DEV-002 — Rule 30.0.1: C library I/O for debug logging

**Rule (Required), paraphrased:** the C library's input/output functions
(<cstdio>) must not be used.

**Location:** `engine/core/log.cpp`, anonymous-namespace `Write()`.

**Rationale:** The debug logger must be dependency-free (no iostreams, no
formatting library) and provably allocation-free; `fputs` to
`stdout`/`stderr` is the minimal facility. No format strings are used
(Rule 21.10.1-adjacent risk avoided), no files are opened, and no streams
are read.

**Risk mitigation:**
- The code is compiled only when `VKSC_ENABLE_LOGGING` is defined (Debug
  builds); production/Release builds contain empty bodies and the
  `MISRA_DEVIATION` block is preprocessed out — no C I/O reaches a
  certified binary.
- Return values are explicitly discarded with `(void)` casts (Rule 0.1.2):
  a logging failure must never abort the engine.

**Verification:** Release builds compile `log.cpp` to empty functions;
`nm`/map inspection confirms no `fputs` reference in the Release engine
library.

---

## Process

1. A deviation is introduced only with: point-of-use `MISRA_DEVIATION()` +
   `// NOLINT`, an entry in this register, and a commit message reference.
2. Deviations are re-reviewed whenever the affected code changes
   materially, and before any certification milestone.
3. Removal of the annotated code requires removing the register entry in
   the same commit.
