#pragma once

// engine::net::Socket — non-blocking UDP socket wrapper.
//
// Phase 4a. First net I/O primitive. Everything else in Phase 4 (packet
// header + ack, channels, fragmentation, congestion) sits on top of this.
//
// Design notes:
//   - RAII: ctor/factory opens the fd, dtor closes it. No double-close.
//   - Non-blocking only. `send` and `recv` never block; they report
//     `WouldBlock` instead, so the caller (the per-tick net pump) can
//     move on.
//   - No exceptions. All failure paths are enum-coded. This is a
//     hot-path primitive.
//   - Host-order addressing in the public API; byte-order swaps happen
//     only at the syscall boundary in the .cpp. Caller code never has
//     to think about `htons` / `ntohs`.
//
// Not in this commit (follow-ups):
//   - Windows / Winsock. This impl is POSIX (Linux/macOS) only; building
//     on Windows will hit a #error in the .cpp until we add the
//     Winsock backend.
//   - Batched syscalls (`recvmmsg` / `sendmmsg`). The loop version is
//     a drop-in optimisation once we can measure syscall overhead.
//
// See docs/design/0013-udp-socket.md.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace engine::net {

// An IPv4 endpoint: address + port, both in HOST byte order.
//
// Stored in host order so arithmetic and comparison are readable in
// logs and tests. The socket layer converts to network order at the
// syscall boundary.
struct Endpoint {
    std::uint32_t address = 0;  // e.g. 0x7F000001 for 127.0.0.1
    std::uint16_t port    = 0;

    // 127.0.0.1:<port>
    static Endpoint loopback(std::uint16_t port);

    // Parses "A.B.C.D:PORT". Returns std::nullopt on bad input.
    // Deliberately narrow: no DNS, no IPv6, no whitespace tolerance.
    static std::optional<Endpoint> parse(std::string_view text);

    // "A.B.C.D:PORT" — convenient for logs and test assertions.
    std::string toString() const;

    bool operator==(const Endpoint& other) const {
        return address == other.address && port == other.port;
    }
    bool operator!=(const Endpoint& other) const { return !(*this == other); }
};

// Result of a non-blocking send.
enum class SendStatus : std::uint8_t {
    Sent,       // payload handed to the kernel
    WouldBlock, // send buffer full; caller should retry later
    Error       // fatal send failure (e.g. invalid socket)
};

// Result of a non-blocking recv.
enum class RecvStatus : std::uint8_t {
    Received,   // a datagram was read into the caller's buffer
    WouldBlock, // no datagram available
    Error       // fatal recv failure
};

struct RecvResult {
    RecvStatus   status = RecvStatus::WouldBlock;
    std::size_t  bytes  = 0;         // valid iff status == Received
    Endpoint     from{};             // valid iff status == Received
};

// Non-blocking UDP socket. Move-only, RAII over the underlying fd.
class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();

    UdpSocket(const UdpSocket&)            = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    // Creates a non-blocking UDP socket bound to the given port on all
    // interfaces (INADDR_ANY). Pass `port = 0` to let the kernel pick
    // an ephemeral port; use `localEndpoint()` afterwards to find out
    // which.
    //
    // Returns std::nullopt if socket()/bind()/fcntl() fails.
    static std::optional<UdpSocket> bind(std::uint16_t port);

    // Whether this socket holds a valid fd.
    bool isOpen() const { return fd_ >= 0; }

    // The local endpoint this socket is bound to. address field is 0
    // (INADDR_ANY) in this implementation; only `port` is meaningful.
    Endpoint localEndpoint() const { return local_; }

    // Non-blocking send. Does NOT partial-send: a UDP datagram either
    // goes out whole or not at all. `bytes` is the total datagram size.
    SendStatus send(const Endpoint& to, const void* data, std::size_t bytes);

    // Non-blocking recv. On success, `capacity` bytes at most are
    // written into `out`; the true payload size is in the result
    // (`RecvResult::bytes`).
    //
    // NOTE: UDP truncates silently if the datagram is larger than
    // `capacity`. Callers should size their buffers to the MTU-aware
    // maximum they expect (Phase 4d will formalise this).
    RecvResult recv(void* out, std::size_t capacity);

private:
    int      fd_    = -1;
    Endpoint local_{};
};

}  // namespace engine::net
