# Learning Notes

> Personal reflection notes — one per meaningful commit or session. The goal
> is to build a body of worked examples of **clean-code principles and
> architecture thinking** you can revisit later.
>
> Unlike [design notes](../design/), learning notes are informal and in the
> first person. Write them for future-you, not for a reader.

---

## How to write one

Copy [TEMPLATE.md](TEMPLATE.md) to `NNNN-kebab-case-title.md` and fill it in
**right after you finish the work, while the decisions are still warm**. If
you skip this, the note is worth much less — you'll only remember the final
answer, not the wrong turns.

Each note should:

- Be short. A page, tops.
- Name the **principle(s)** the work demonstrated (SRP, dependency inversion,
  YAGNI, data-oriented design, etc.). Cite a book/article if one influenced you.
- Honestly record what you **got wrong first** and how you noticed.
- Link to the [design note](../design/) and the commit it refers to.

See [../CODE_DOCS.md](../CODE_DOCS.md) for the broader documentation system.

---

## Index

| # | Title | Principle(s) | Related |
|---|---|---|---|
| [0001](0001-separating-concerns.md) | Separating concerns: why `Engine` had to split | SRP, Dependency Rule, Decoupling via data | [design/0001](../design/0001-simulation-split.md) |
| [0002](0002-data-as-a-seam.md) | Data as a seam: `PlayerInput` instead of `IInputSource` | Dependency Rule, SRP, YAGNI, data-oriented, composition over inheritance | [design/0002](../design/0002-decouple-inputsystem.md) |
| [0003](0003-headless-first.md) | Headless-first: making the simulation exist without a window | SRP, Dependency Rule, separation of clocks, YAGNI | [design/0003](../design/0003-extract-simulation.md) |
| [0004](0004-polymorphism-for-sameness.md) | Polymorphism should represent sameness, not history | LSP, prefer free functions, YAGNI | [design/0004](../design/0004-render-as-free-function.md) |
| [0005](0005-retiring-the-god-class.md) | Retiring the god-class: deleting `Engine` outright | SRP, namespaces as a design tool, delete-over-alias | [design/0005](../design/0005-introduce-clientapp.md) |
| [0006](0006-the-build-graph-is-the-architecture.md) | The build graph is the architecture | Enforce invariants in the build, directory ≠ dependency graph | [design/0006](../design/0006-cmake-split.md) |
| [0007](0007-test-the-invariant.md) | Test the invariant, not the implementation | Test the contract not the innards, determinism as a contract | [design/0007](../design/0007-determinism-test.md) |
| [0008](0008-wire-format-discipline.md) | Wire-format discipline: build the byte layer before the protocol | Bottom-up when the bottom constrains you, YAGNI inverted | [design/0008](../design/0008-bitstream.md) |
| [0009](0009-quantization-throwing-away-bits.md) | Quantisation: throwing away bits you never needed | Pay only for the bits you use, centralise wire constants | [design/0009](../design/0009-quantization.md) |
| [0010](0010-traits-over-branches.md) | Traits over branches: a policy-free encoder | Mechanism vs policy, open/closed via templates, compile-time errors over runtime crashes | [design/0010](../design/0010-snapshot-format.md) |
| [0011](0011-the-bug-fix-was-the-point.md) | The bug fix that was the whole point | Equality at the contract level, asymmetric ops, tests catch intent bugs | [design/0011](../design/0011-delta-encoding.md) |
| [0012](0012-measurement-beats-opinion.md) | Measurement beats opinion | Ship the measurement then decide, document the miss, hot-path first | [design/0012](../design/0012-benchmarks.md) |
| [0013](0013-smallest-socket.md) | The smallest socket that flows a packet | Scope discipline, documented deferrals, host-order at the API | [design/0013](../design/0013-udp-socket.md) |
| [0014](0014-ack-is-a-coordinate-system.md) | The ack bitfield is a coordinate system | Unify lookalikes, sequence arithmetic is a custom op, inject time | [design/0014](../design/0014-ack-and-rtt.md) |
| [0015](0015-three-channels-one-idea.md) | Three channels, one idea | Different signatures → different classes; tag, don't time; contrast teaches | [design/0015](../design/0015-channels.md) |
| [0016](0016-the-buffer-you-dont-allocate.md) | The buffer you don't allocate | Bound the work a stranger can make you do; named caps; strict parsing | [design/0016](../design/0016-fragmentation.md) |
| [0017](0017-two-modes-beat-a-curve.md) | Two modes beat a curve | Prefer a finite state machine with named knobs over continuous controllers; hysteresis as a feature | [design/0017](../design/0017-congestion-control.md) |
| [0018](0018-reproducibility-is-a-feature.md) | Reproducibility is a feature | Stable RNG draws across config sweeps; seeds aren't enough on their own | [design/0018](../design/0018-network-simulator.md) |

*(new rows append here as notes land)*
