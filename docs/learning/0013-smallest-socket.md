# 0013 — The smallest socket that flows a packet

- **Date:** 2026-04-19
- **Commit:** Phase 4a — UDP socket wrapper
- **Related:** [design/0013](../design/0013-udp-socket.md)
- **Principles:** Cross the boundary once, pick a scope and hold it, tests that talk to the real kernel

---

## What I built

A non-blocking `engine::net::UdpSocket` over POSIX sockets: RAII fd,
`bind` / `send` / `recv`, a host-order `Endpoint`, two small enums for
status. ~180 lines of .cpp + a test file that binds two sockets on
loopback and exchanges a datagram.

## Why (the problem)

Phase 4 is the hardest phase on the roadmap. Everything in it —
sequence numbers, ack bitfields, channels, fragmentation, congestion
control, simulated latency — is written *on top of* a socket
primitive. If that primitive is wrong or fiddly, every layer above
inherits the pain.

Question I asked: **what is the smallest socket wrapper that I can
build a real ack system on next?**

## How I approached it

Two recurring temptations during Phase 4a:

### Temptation 1: do Windows now

The roadmap literally says "Linux + Windows". Doing both "while I'm
here" felt responsible. It's not. I don't have a Windows machine to
run against, and Winsock has enough subtle differences
(`WSAStartup`, `closesocket`, its own errno, unusual non-blocking
mechanics) that writing it blind is a guaranteed-bug-per-100-lines
exercise. Writing it now would be LARPing as a cross-platform
project.

Decision: Linux/macOS only, with a hard `#error` in the .cpp so
nobody is surprised. Portability deferred, cleanly.

This is the first time in this project I've said "the roadmap asks
for X, I'm doing X-minus-part." Recording it here so I don't slip
into thinking every bullet must land as-written.

### Temptation 2: the swiss-army-knife socket

I caught myself wanting to add: timeouts on recv, batched
`recvmmsg`, a send queue, a "connected" UDP mode, per-peer stats.
None of those are needed to write an ack system. Several of them are
actively wrong to bake into the primitive (batching belongs in a
higher layer once we can measure).

Rule I held myself to: **if the next commit doesn't need it, don't
write it.** The ack system needs `send` and `recv`. That's what
shipped.

## The one design call worth a paragraph: host-order endpoints

POSIX socket code is a parade of `htons` / `ntohs` / `htonl` /
`ntohl`. Every byte-swap site is a place to make a mistake. The
options:

1. **Network order everywhere.** Matches the syscalls; logs are
   unreadable; arithmetic breaks assumptions.
2. **Host order at the API, network order at the syscall
   boundary.** Byte swaps happen once, in the `.cpp`, right before
   `sendto` / right after `recvfrom`.

I picked (2). It means `Endpoint::loopback(port)` just returns
`{0x7F000001, port}`, tests compare ports as integers, logs read as
`127.0.0.1:12345`. The byte-swap cost is nothing. The mental
overhead saved is substantial.

## Tests that touch the kernel

I went back and forth on whether the loopback send/recv test belongs
in `unit_tests` at all. Real kernel I/O isn't a "unit" test by the
classical definition — the kernel is a dependency. But:

- The primitive's *job* is to talk to the kernel. A mock test proves
  nothing.
- Loopback tests are fast (sub-ms) and reliable on every dev machine
  I'll ever use.
- If they flake in CI, that's a signal; I'll split them into a
  separate `net_tests` target then.

Kept them together. Revisit if reality disagrees.

One concrete trap in writing the test: I wrote a single `recv` call
expecting the datagram to be there immediately after `send` returns.
It wasn't always — loopback is fast but not instant when the
scheduler chooses to queue the packet. Added a short retry loop (50
× 1 ms). That's the polling shape the real net pump will use too, so
the test is honest about the eventual-consistency of the socket.

## Principle(s) this demonstrates

- **Pick the smallest scope and hold it.** The socket is not the ack
  system, not the channel layer, not the congestion controller. It
  is a fd wrapper. Every line beyond that is debt.
- **Cross the error boundary once.** Byte order, errno, fd lifetime —
  all of those translate at one spot in the .cpp and never leak into
  callers. The rest of the engine shouldn't know POSIX exists.
- **Deferring is a decision, document it.** Windows deferred. `mmsg`
  deferred. A future me opening this ADR will know the deferrals are
  deliberate, not forgotten.

## What I got wrong first

- First draft had `recv` returning `std::optional<size_t>`. That
  conflated "no data" (routine) with "socket dead" (fatal). Replaced
  with a `RecvStatus` enum. Unambiguous, grep-able.
- I originally wrote `bind(port)` to bail on `EADDRINUSE`. Dropped
  `SO_REUSEADDR` → test run → re-run → flake on TIME_WAIT. Added
  `SO_REUSEADDR`. Noted in the ADR so I don't "clean that up" later
  without thinking.
- Tried to return the bound port from a mutable out-param. Cleaner
  to expose `localEndpoint()` and let the caller query it whenever.

## Takeaway for future me

- When in doubt about scope, ask: *does the next commit need it?* If
  no, the answer is no.
- Platform abstraction lives entirely in the .cpp. The header has no
  `#ifdef _WIN32`. That's the bar.
- Host-order at the API, network-order at the syscall. This is a
  reusable rule for any protocol primitive that involves byte-swapping.
- Integration tests against the real kernel are allowed and useful
  *as long as* the primitive under test exists precisely to talk to
  the kernel.
