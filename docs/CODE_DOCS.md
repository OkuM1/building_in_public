# Code Documentation Conventions

> How we document this codebase so that future-you (and anyone reading the repo)
> can understand **what** the code does, **why** it was built that way, and
> **what was learned** from building it.
>
> Three layers, each small and deliberate:
>
> 1. **Doxygen-style comments** on headers — the *what* (API contract).
> 2. **Design notes** under [`design/`](design/) — the *why* (decisions, alternatives).
> 3. **Learning notes** under [`learning/`](learning/) — the *how it taught me* (principles, reflections).
>
> Every non-trivial change touches all three. See [design/README.md](design/README.md)
> and [learning/README.md](learning/README.md).

---

## 1. Doxygen header comments

**Goal:** someone reading a header should know what each public symbol does
without opening the `.cpp`. Keep it short. Don't restate the name.

### What to document

- Every public class, struct, function, and constant.
- Private members: only when non-obvious.
- Files: one-line `@file` + `@brief` at the top, plus an `@see` link to the
  governing design note.

### Tags we use

| Tag | Meaning |
|---|---|
| `@file`    | File name (Doxygen uses it to index the file). |
| `@brief`   | One-sentence summary. |
| `@param`   | One per parameter, including constraints. |
| `@return`  | What the return value means. |
| `@pre` / `@post` | Preconditions / postconditions when important. |
| `@see`     | Cross-references — sibling symbol, **design note**, or learning note. |
| `@note`    | Non-obvious behaviour or gotcha. |
| `@warning` | Something that will bite if ignored. |

### Example — a small header, fully commented

```cpp
#pragma once
#include <cstdint>
#include <limits>

/**
 * @file Entity.h
 * @brief Entity identifiers and invariants for the ECS.
 * @see docs/design/0001-simulation-split.md
 * @see docs/design/README.md — design-note index
 */

namespace engine {

/// @brief Opaque handle to an entity. Entities carry no data of their own;
///        all data lives in components keyed by this id.
using EntityId = uint32_t;

/// @brief Sentinel value meaning "no entity". Returned by APIs that may fail
///        to resolve an entity (e.g. target lookup).
constexpr EntityId INVALID_ENTITY = std::numeric_limits<EntityId>::max();

/// @brief Compile-time upper bound on live entities. Chosen so that
///        per-component packed arrays fit in L2 for small components
///        (see design note 0002 when it lands).
constexpr EntityId MAX_ENTITIES = 10000;

} // namespace engine
```

### Example — a function

```cpp
/**
 * @brief Advance the simulation by real wall-clock time.
 *
 * Accumulates @p realDt and runs zero or more fixed-timestep @ref step calls
 * until the accumulator is drained below @ref FIXED_DT. Safe against the
 * "spiral of death": input larger than 0.25s is clamped.
 *
 * @param realDt  Seconds elapsed since last call. Must be >= 0.
 * @return        Interpolation alpha in [0, 1). Pass to the renderer to
 *                blend between previous and current transforms.
 *
 * @see step()
 * @see docs/design/0001-simulation-split.md
 */
float advance(float realDt);
```

### What **not** to do

- Don't doxygen-comment obvious getters unless behaviour is non-obvious.
- Don't duplicate the function body in prose ("adds 1 to x" — we can read).
- Don't write novels. If a paragraph is needed, it belongs in a **design note**,
  linked via `@see`.

---

## 2. Design notes (the *why*)

Permanent, numbered markdown files under [`design/`](design/) following a
minimal ADR format: **Context → Decision → Alternatives → Consequences →
Code pointers**. One per meaningful decision.

See [design/TEMPLATE.md](design/TEMPLATE.md) and the index in
[design/README.md](design/README.md).

Every code file governed by a design note links to it from its `@file`
comment, and the design note lists the files in its "Code pointers" section.

---

## 3. Learning notes (the *reflection*)

Short personal notes under [`learning/`](learning/) — one per commit or
focused session. Purpose: build a body of worked examples of clean-code
principles and architecture trade-offs you can revisit.

See [learning/TEMPLATE.md](learning/TEMPLATE.md) and the index in
[learning/README.md](learning/README.md).

---

## 4. Generating the API reference

A minimal `Doxyfile` lives at the repo root. To generate HTML locally:

```bash
doxygen Doxyfile       # writes to build/docs/html/
xdg-open build/docs/html/index.html
```

CI will build this on every push to `master` (eventually — tracked in the
roadmap).

---

## 5. The commit workflow (where this all ties together)

For every non-trivial change:

1. **Before coding:** write or update the **design note** (at least Context +
   Decision). This forces you to think before typing.
2. **While coding:** add Doxygen comments to new/changed public symbols.
   Put `@see docs/design/NNNN-*.md` at the top of each governed file.
3. **After coding:** write the **learning note** — what you built, what
   principle it shows off, what you'd do differently.
4. **Commit:** conventional-commits style, body mentioning the design note
   number. One logical change per commit.

Result: `git log` is a table of contents over the design and learning notes,
and every code file has a clickable trail back to the reasoning that produced it.
