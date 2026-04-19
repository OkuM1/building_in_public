# 0013 — Phase 4a: non-blocking UDP socket wrapper

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 4 — Reliable UDP (sub-commit 4a / 6)
- **Related:** [ROADMAP §Phase 4](../ROADMAP.md), [0006 cmake-split](0006-cmake-split.md)

## Context

Phase 3 gave us snapshot bytes. Phase 4 has to get those bytes from
server to client over UDP, reliably where we want and unreliably
where we don't. Before any of that — ack bitfields, channels,
fragmentation, congestion — we need the primitive that actually
touches the network: a non-blocking UDP socket.

Everything later in Phase 4 sits on this class. Getting it wrong is
expensive; getting it over-engineered is also expensive. Goal for
this commit: the smallest possible socket that lets us send and
receive one datagram on loopback, exercised by an integration test
against the real kernel.

## Decision

### Surface

```cpp
namespace engine::net {
    struct Endpoint { uint32_t address; uint16_t port; };   // host order
    enum class SendStatus { Sent, WouldBlock, Error };
    enum class RecvStatus { Received, WouldBlock, Error };

    class UdpSocket {
        static std::optional<UdpSocket> bind(uint16_t port);
        SendStatus send(const Endpoint&, const void*, size_t);
        RecvResult recv(void*, size_t capacity);
    };
}
```

Move-only, RAII over a POSIX fd. Dtor closes the fd; assignment closes
the old one. `bind(0)` lets the kernel pick an ephemeral port, read
back via `localEndpoint()`.

### Host-order `Endpoint`

Addresses and ports are stored in host byte order in the public API.
Byte swaps happen only at the `sendto`/`recvfrom`/`bind` boundary in
the .cpp. Rationale:

- Tests and logs read naturally (`127.0.0.1:12345`, not `0x39300000`).
- Every new caller not needing to remember `htons`/`ntohs` is a class
  of bug that cannot be written.
- The byte-swap cost is a few instructions per datagram. Irrelevant
  vs the syscall.

### Non-blocking only, enum-coded failures

- `fcntl(F_SETFL, O_NONBLOCK)` after `socket()`; every send/recv
  reports `WouldBlock` instead of stalling.
- No exceptions. The net pump runs every tick; it must never throw.
- Caller distinguishes "try again next tick" (`WouldBlock`) from
  "socket is broken, tear down the connection" (`Error`).

### `SO_REUSEADDR` on by default

Test processes bind → run → exit → bind again in a tight loop. Without
`SO_REUSEADDR` this flakes on TIME_WAIT. In Phase 5 or 6 we may want a
separate "production" ctor that disables it; not worth the complexity
today.

### POSIX only — Winsock deferred

The roadmap's Phase 4a asks for Linux + Windows. This commit does
Linux/macOS only; Windows gets a `#error` in the .cpp directing the
reader to this ADR. Reasons:

1. The target dev + deploy OS is Linux. Windows is a distant third.
2. Winsock has a meaningfully different API (WSAStartup, `closesocket`,
   `WSAEWOULDBLOCK`, …) — writing it without a Windows machine to
   test is a way to ship a bug.
3. The abstraction boundary is correct: the header stays portable,
   only the .cpp branches. Adding Winsock later is a closed, bounded
   change.

### `recvmmsg` / `sendmmsg` deferred

The current impl calls `recvfrom` / `sendto` once per datagram. On
Linux, `recvmmsg` / `sendmmsg` batch up to N datagrams into one
syscall — typically a 2–5× throughput win on loaded servers. Out of
scope here. The trigger for adding them is either the Phase 4
benchmarks showing syscall overhead, or hitting the 100-clients-per-VPS
scaling target in Phase 6.

## Alternatives considered

1. **Boost.Asio.** Industrial, well-tested, but it drags a huge
   dependency for a 150-line POSIX wrapper. And the learning goal of
   this project explicitly calls for doing the syscalls ourselves.
2. **A `std::expected`-like `Result<T, Error>` return type.** Clean on
   paper but we're on C++17 and I don't want to write a monadic
   result type before the first datagram flows. Two enums is fine.
3. **Exceptions on bind failure.** Would be ergonomic at process start
   but the same code path runs in reconnect paths later, where throws
   are wrong. `std::optional<UdpSocket>` covers both uses.

## Consequences

- New TU in `simulation`: `src/engine/net/Socket.cpp`. The simulation
  target now depends on POSIX sockets, but those are part of libc on
  Linux — no new CMake link line needed.
- `unit_tests` grows 7 cases (32 assertions): endpoint parsing,
  ephemeral bind, `WouldBlock` on empty socket, loopback send/recv,
  move-construct.
- Tests do real kernel I/O. They're fast on loopback but they are, by
  definition, not hermetic. Acceptable at this scale; revisit if CI
  flakes.
- Headless invariant intact: `simulation` still links no GL libraries.

## Code pointers

- [include/engine/net/Socket.h](../../include/engine/net/Socket.h)
- [src/engine/net/Socket.cpp](../../src/engine/net/Socket.cpp)
- [tests/test_socket.cpp](../../tests/test_socket.cpp)

## Next

- Phase 4b: `PacketHeader` (sequence + ack + 32-bit ack bitfield) and
  smoothed RTT. That's where "reliable UDP" actually starts meaning
  something.
