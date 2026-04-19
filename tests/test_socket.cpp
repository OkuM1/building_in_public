// Phase 4a: UDP socket smoke tests (loopback, no simulated network).
//
// These are integration tests against the real kernel, not mocks. They
// exercise socket(), bind(), sendto(), recvfrom() for engine::net::UdpSocket.

#include <doctest.h>

#include <array>
#include <cstring>
#include <thread>
#include <chrono>

#include "engine/net/Socket.h"

using namespace engine::net;

TEST_CASE("Endpoint::loopback produces 127.0.0.1") {
    const auto ep = Endpoint::loopback(12345);
    CHECK(ep.address == 0x7F000001u);
    CHECK(ep.port == 12345);
    CHECK(ep.toString() == "127.0.0.1:12345");
}

TEST_CASE("Endpoint::parse round-trips valid IPv4:port strings") {
    const auto ep = Endpoint::parse("10.0.0.42:5678");
    REQUIRE(ep.has_value());
    CHECK(ep->address == 0x0A00002Au);
    CHECK(ep->port == 5678);
    CHECK(ep->toString() == "10.0.0.42:5678");
}

TEST_CASE("Endpoint::parse rejects malformed input") {
    CHECK_FALSE(Endpoint::parse("").has_value());
    CHECK_FALSE(Endpoint::parse("no-colon").has_value());
    CHECK_FALSE(Endpoint::parse(":8080").has_value());
    CHECK_FALSE(Endpoint::parse("127.0.0.1:").has_value());
    CHECK_FALSE(Endpoint::parse("127.0.0.1:0").has_value());     // port 0 disallowed for peers
    CHECK_FALSE(Endpoint::parse("127.0.0.1:99999").has_value()); // port > 65535
    CHECK_FALSE(Endpoint::parse("not.an.ip:1234").has_value());
}

TEST_CASE("UdpSocket::bind with port=0 assigns an ephemeral port") {
    auto sock = UdpSocket::bind(0);
    REQUIRE(sock.has_value());
    CHECK(sock->isOpen());
    CHECK(sock->localEndpoint().port != 0);
}

TEST_CASE("UdpSocket::recv on empty socket returns WouldBlock") {
    auto sock = UdpSocket::bind(0);
    REQUIRE(sock.has_value());

    std::array<std::uint8_t, 64> buf{};
    const auto r = sock->recv(buf.data(), buf.size());
    CHECK(r.status == RecvStatus::WouldBlock);
    CHECK(r.bytes == 0);
}

TEST_CASE("UdpSocket loopback: send from A to B, recv at B") {
    auto a = UdpSocket::bind(0);
    auto b = UdpSocket::bind(0);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());

    const auto bAddr = Endpoint::loopback(b->localEndpoint().port);

    const char        payload[] = "hello-udp";
    constexpr std::size_t payloadLen = sizeof(payload) - 1;

    CHECK(a->send(bAddr, payload, payloadLen) == SendStatus::Sent);

    // Loopback is reliable but scheduling isn't instantaneous. Give the
    // kernel a moment, then retry a few times.
    std::array<std::uint8_t, 64> buf{};
    RecvResult r{};
    for (int i = 0; i < 50; ++i) {
        r = b->recv(buf.data(), buf.size());
        if (r.status == RecvStatus::Received) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(r.status == RecvStatus::Received);
    CHECK(r.bytes == payloadLen);
    CHECK(std::memcmp(buf.data(), payload, payloadLen) == 0);

    // The sender's endpoint should be on loopback, using A's bound port.
    CHECK(r.from.address == 0x7F000001u);
    CHECK(r.from.port == a->localEndpoint().port);
}

TEST_CASE("UdpSocket move transfers ownership") {
    auto a = UdpSocket::bind(0);
    REQUIRE(a.has_value());
    const auto port = a->localEndpoint().port;

    UdpSocket moved = std::move(*a);
    CHECK(moved.isOpen());
    CHECK(moved.localEndpoint().port == port);
    CHECK_FALSE(a->isOpen());  // NOLINT(bugprone-use-after-move): intentional
}
