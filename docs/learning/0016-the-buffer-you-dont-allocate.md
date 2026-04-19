# 0016 — The buffer you don't allocate

- **Date:** 2026-04-19
- **Commit:** Phase 4d — fragmentation & reassembly
- **Related:** [design/0016](../design/0016-fragmentation.md), [learning/0015](0015-three-channels-one-idea.md)
- **Principles:** Bound the work a stranger can make you do, named caps > magic numbers, write the attacker test

---

## What I built

`fragment(msgId, data, bytes, maxFragBody)` splits a payload into
fragments with a 6-byte header each. `Reassembler::offer(bytes, now)`
collects fragments and returns the reassembled payload when complete.
`gc(now, timeoutMs)` drops stale partial sets. 10 new tests.

## Why (the problem)

A Phase 3 snapshot can be 15 KB. Kernel-level IP fragmentation on UDP
has a well-known failure mode: one dropped IP fragment silently
kills the whole datagram, with no signal. Every real game engine
solves this at the application layer.

This commit was mostly straightforward — fragment index, fragment
total, a map keyed by message id, reassemble on the last piece.
What's interesting about it is what I had to think about around the
happy path.

## How I approached it

### The adversarial mindset kicked in early

The first reassembler implementation I sketched was: one
`unordered_map<messageId, InFlight>`. Entries live forever if
fragments stop arriving. That's Wednesday's implementation.

Then I wrote the test for it. Then I asked: what does a hostile peer
do? Answer: send one fragment each of 65 535 different message ids
over a few minutes. The map grows without bound. Memory OOM is the
attack surface.

Three knobs fell out of that one question:

1. **`kMaxInFlight = 64`** — hard cap on concurrent partial messages.
   When exceeded, evict the oldest. The attacker's 65 535 distinct
   message ids now just churn the eviction queue; we bound memory.
2. **`kMaxFragmentsPerMessage = 256`** — the attacker can claim
   `fragTotal = 65535` in a fragment header, making us pre-allocate
   huge arrays. Cap it.
3. **`kDefaultTimeoutMs = 2000`** — caller-driven `gc()` drops
   partial sets that never complete. Belt to the `kMaxInFlight`
   suspenders.

The principle under these: **bound the work a stranger can make you
do.** Any code path the peer can invoke with attacker-chosen
parameters needs a cap on memory, time, or CPU. Every single one.
Write out the caps as named constants, don't hide them as magic
numbers in the middle of a function.

I'm going to be religious about this for the rest of Phase 4. The
network is the ultimate untrusted input.

### Named caps are documentation

I put `kMaxInFlight`, `kMaxFragmentsPerMessage`, and
`kDefaultTimeoutMs` in the header, not as private constants in the
`.cpp`. Two reasons:

- A reviewer reading the header sees the limits immediately. No
  "where's this `64` coming from?"
- Tuning is one-line. Someone tracking memory in production can
  change the cap and re-run benchmarks.

Compare against "sprinkle `64` in three places throughout the
`.cpp`." The named-constant version is harder to misuse.

### What the `fragTotal` contract is really doing

Early draft of `offer()` accepted any `fragTotal` for an incoming
fragment and silently updated the stored value if it changed. This
is wrong but not obviously wrong — "just use the new one" feels
forgiving.

What it actually enables: an attacker sends fragment 0 of 3 (we
allocate a 3-slot array), then fragment 99 of 100 (we resize to 100,
fill slot 99), then fragment 42 of 200 (resize to 200, fill slot 42).
By the end we've wasted several allocations and our `received` count
is probably wrong too.

The correct read: **`fragTotal` is part of the message's identity.**
If it disagrees across fragments of the same `messageId`, that's a
protocol violation, full stop. Discard the in-flight entry and start
clean (or refuse entirely). Wrote a test that exercises exactly this.

### "Malformed input" must not crash

`offer()` takes `const uint8_t* data, size_t bytes`. Untrusted. If
the caller passes `bytes < 6`, `readFragmentHeader` must notice. If
the header claims `fragIndex >= fragTotal`, we must notice. If
`fragTotal == 0`, we must notice.

All three are tested. None of them affect my happy-path speed — a
few comparisons per fragment isn't a hot loop. Skipping them is how
you ship a CVE.

## Principle(s) this demonstrates

- **Caps go in the header with a name.** Every attacker-tunable knob
  gets a `kMaxX` constant. Reviewers see them, future-you can tune
  them, and there are no hidden magic numbers.
- **"Forgiving" parsers are how you get hacked.** Strict validation
  on fragmented / framed input. Disagreement with prior fragments is
  corruption; drop, don't merge.
- **The adversarial test isn't a nice-to-have.** Running through
  "what does a hostile peer do?" before writing the first line of
  tests is what surfaced the three caps.
- **Inject time, even in primitives.** Reassembler's expiry is
  caller-driven via `gc(nowMs, timeoutMs)`. Tests step time by hand.
  No `sleep_for`, no clock global.

## What I got wrong first

- First draft of the fast-path single-fragment case still inserted
  into the `inFlight_` map and then immediately erased it. Cleaner
  and cheaper to bypass the map entirely when `fragTotal == 1`.
- I initially stored fragments in a pre-sized `vector<uint8_t>`
  indexed by `fragIndex * maxFragBody`. This means an attacker
  claiming `fragTotal = 256, maxFragBody = 1200` pre-allocates 300 KB
  per message. Switched to `vector<FragmentPayload> pieces` so the
  memory follows actual arrivals.
- My "duplicate fragment" test originally only checked `inboxSize`
  behaviour. Rewrote to check that sending fragment 0 twice doesn't
  double-count `received` (which would have completed the message
  with only two of three pieces). Good catch of mine: the bug would
  have been a silent truncation of the reassembled payload.

## Takeaway for future me

- Treat every incoming network field as attacker-controlled until
  bounded. `u16 fragTotal` → bound. `messageId` → can't bound, so
  bound `kMaxInFlight`. Chain of reasoning, not a single check.
- Named caps. `kMaxX = N` in a header is more maintainable than `if
  (n > 64)`.
- Pre-allocating on attacker-specified sizes is an OOM bug. Grow on
  actual arrivals, not on claimed size.
- Parse strictly. Reject disagreement between fragments. Don't patch
  up "probably fine" protocol ambiguity with forgiving merges.
