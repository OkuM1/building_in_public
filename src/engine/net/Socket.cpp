// engine::net::UdpSocket — POSIX implementation.
//
// Windows / Winsock is explicitly deferred; see docs/design/0013-udp-socket.md.

#include "engine/net/Socket.h"

#if defined(_WIN32)
#  error "engine::net::UdpSocket: Windows (Winsock) backend not yet implemented. \
Track it in docs/design/0013-udp-socket.md follow-ups."
#endif

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

namespace engine::net {

// ---------------------------------------------------------------------------
// Endpoint
// ---------------------------------------------------------------------------

Endpoint Endpoint::loopback(std::uint16_t port) {
    return Endpoint{0x7F000001u, port};  // 127.0.0.1
}

std::optional<Endpoint> Endpoint::parse(std::string_view text) {
    // Expect "A.B.C.D:PORT". Keep parsing strict — no whitespace, no
    // surrounding brackets, no IPv6. This is an engine-internal wire
    // helper, not a user-facing URL parser.
    const auto colon = text.find(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 >= text.size()) {
        return std::nullopt;
    }

    // IPv4 via inet_pton, which wants a NUL-terminated string.
    char hostBuf[16];  // max "255.255.255.255" + NUL
    if (colon >= sizeof(hostBuf)) return std::nullopt;
    std::memcpy(hostBuf, text.data(), colon);
    hostBuf[colon] = '\0';

    in_addr addr{};
    if (inet_pton(AF_INET, hostBuf, &addr) != 1) return std::nullopt;

    // Port: std::from_chars would be nicer but is overkill here.
    unsigned long parsedPort = 0;
    for (std::size_t i = colon + 1; i < text.size(); ++i) {
        char c = text[i];
        if (c < '0' || c > '9') return std::nullopt;
        parsedPort = parsedPort * 10 + static_cast<unsigned>(c - '0');
        if (parsedPort > 0xFFFFu) return std::nullopt;
    }
    if (parsedPort == 0) return std::nullopt;  // port 0 is not a valid peer

    Endpoint out;
    out.address = ntohl(addr.s_addr);  // store in host order
    out.port    = static_cast<std::uint16_t>(parsedPort);
    return out;
}

std::string Endpoint::toString() const {
    // snprintf is fine here — this is log/test formatting, not hot path.
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u",
                  (address >> 24) & 0xFF,
                  (address >> 16) & 0xFF,
                  (address >> 8) & 0xFF,
                  address & 0xFF,
                  port);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// UdpSocket
// ---------------------------------------------------------------------------

UdpSocket::~UdpSocket() {
    if (fd_ >= 0) ::close(fd_);
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : fd_(other.fd_), local_(other.local_) {
    other.fd_    = -1;
    other.local_ = {};
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        fd_          = other.fd_;
        local_       = other.local_;
        other.fd_    = -1;
        other.local_ = {};
    }
    return *this;
}

std::optional<UdpSocket> UdpSocket::bind(std::uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return std::nullopt;

    // Non-blocking.
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(fd);
        return std::nullopt;
    }

    // Allow quick re-bind in tests / server restarts.
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return std::nullopt;
    }

    // Resolve the actual bound port (important when `port == 0`).
    sockaddr_in bound{};
    socklen_t   boundLen = sizeof(bound);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &boundLen) < 0) {
        ::close(fd);
        return std::nullopt;
    }

    UdpSocket s;
    s.fd_           = fd;
    s.local_.address = 0;                        // INADDR_ANY
    s.local_.port    = ntohs(bound.sin_port);
    return s;
}

SendStatus UdpSocket::send(const Endpoint& to, const void* data, std::size_t bytes) {
    if (fd_ < 0) return SendStatus::Error;

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(to.address);
    addr.sin_port        = htons(to.port);

    const ssize_t sent = ::sendto(fd_, data, bytes, 0,
                                  reinterpret_cast<const sockaddr*>(&addr),
                                  sizeof(addr));
    if (sent == static_cast<ssize_t>(bytes)) return SendStatus::Sent;

    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return SendStatus::WouldBlock;
        return SendStatus::Error;
    }

    // A short send on UDP would mean kernel truncated the datagram,
    // which shouldn't happen within MTU. Surface it as an error.
    return SendStatus::Error;
}

RecvResult UdpSocket::recv(void* out, std::size_t capacity) {
    RecvResult r;
    if (fd_ < 0) {
        r.status = RecvStatus::Error;
        return r;
    }

    sockaddr_in src{};
    socklen_t   srcLen = sizeof(src);
    const ssize_t got = ::recvfrom(fd_, out, capacity, 0,
                                   reinterpret_cast<sockaddr*>(&src), &srcLen);

    if (got < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            r.status = RecvStatus::WouldBlock;
        } else {
            r.status = RecvStatus::Error;
        }
        return r;
    }

    r.status       = RecvStatus::Received;
    r.bytes        = static_cast<std::size_t>(got);
    r.from.address = ntohl(src.sin_addr.s_addr);
    r.from.port    = ntohs(src.sin_port);
    return r;
}

}  // namespace engine::net
