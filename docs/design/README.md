# Design Notes

> Numbered, permanent records of non-trivial design decisions. ADR-style:
> each note exists so future-you can answer *"why is it built this way?"*
> without reverse-engineering git blame.
>
> New notes are append-only. When a decision is revisited, write a **new**
> note that supersedes the old one and mark the old one `Superseded by NNNN`.

---

## How to write one

Copy [TEMPLATE.md](TEMPLATE.md) to `NNNN-kebab-case-title.md` (next free
number), fill in the sections, and commit it **alongside the code change it
governs**. Do not write a design note without a matching code change, and do
not make a non-trivial code change without a design note.

See [../CODE_DOCS.md](../CODE_DOCS.md) for the broader documentation system.

### Rules

- One decision per note. Split if in doubt.
- Keep it short: usually 1 page of markdown.
- Record **alternatives considered** even if briefly. The rejected ones are
  often more informative than the chosen one.
- Link to the code files under "Code pointers". Those files link back here
  from their `@file` Doxygen comment.
- Status lifecycle: `Proposed` → `Accepted` → `Implemented` → *(optionally)*
  `Superseded by NNNN`.

---

## Index

| # | Title | Status | Phase |
|---|---|---|---|
| [0001](0001-simulation-split.md) | Split `Engine` into headless `Simulation` + `ClientApp` | Accepted | 2.5 |
| [0002](0002-decouple-inputsystem.md) | Decouple `InputSystem` from GLFW | Implemented | 2.5 |
| [0003](0003-extract-simulation.md) | Extract `engine::Simulation` from `Engine` | Implemented | 2.5 |
| [0004](0004-render-as-free-function.md) | Rendering as a free function, not a `System` | Implemented | 2.5 |
| [0005](0005-introduce-clientapp.md) | Introduce `client::ClientApp`, retire `engine::Engine` | Implemented | 2.5 |
| [0006](0006-cmake-split.md) | Split CMake into `simulation` + `client_lib`, add `server` target | Implemented | 2.5 |
| [0007](0007-determinism-test.md) | Test simulation determinism at the `Simulation` boundary | Implemented | 2.5 |
| [0008](0008-bitstream.md) | `engine::net::BitStream`: bit-level serialization primitive | Implemented | 3 |
| [0009](0009-quantization.md) | Quantised position & angle on the wire | Implemented | 3 |
| [0010](0010-snapshot-format.md) | Snapshot wire format v1 (trait-based encoder) | Implemented | 3 |
| [0011](0011-delta-encoding.md) | Delta encoding: send only what changed | Implemented | 3 |
| [0012](0012-benchmarks.md) | Phase 3 benchmark wiring + published numbers | Implemented | 3 |
| [0013](0013-udp-socket.md) | Phase 4a: non-blocking UDP socket wrapper | Implemented | 4 |
| [0014](0014-ack-and-rtt.md) | Phase 4b: packet header + ack bitfield + smoothed RTT | Implemented | 4 |
| [0015](0015-channels.md) | Phase 4c: channels (unreliable / reliable-unordered / reliable-ordered) | Implemented | 4 |
| [0016](0016-fragmentation.md) | Phase 4d: fragmentation & reassembly | Implemented | 4 |
| [0017](0017-congestion-control.md) | Phase 4e: congestion control (good/bad mode) | Implemented | 4 |
| [0018](0018-network-simulator.md) | Phase 4f: in-process network simulator | Implemented | 4 |

*(new rows append here as notes land)*
