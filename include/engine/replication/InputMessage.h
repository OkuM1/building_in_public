#pragma once

/// @file InputMessage.h
/// @brief Wire format for client→server player input. Phase 5a.
///
/// Every network tick the client serialises one `InputMessage` and sends it
/// on `ChannelId::ReliableUnordered`. The `tick` field stamps the input
/// against the client's simulation clock so the server can schedule it
/// correctly and echo it back in snapshot acks for client reconciliation.
///
/// @par Wire format (varint tick + 6 bool bits)
/// @code
/// input_message :=
///     varint  tick          // client simulation tick (wraps at UINT32_MAX)
///     bool    moveUp        // 1 bit each, packed by BitWriter
///     bool    moveDown
///     bool    moveLeft
///     bool    moveRight
///     bool    attack
///     bool    dodge
/// @endcode
///
/// @see docs/design/0020-replication-model.md

#include <cstdint>

#include "engine/net/BitStream.h"
#include "game/components/GameComponents.h"

namespace engine::replication {

/// An input packet sent from client to server once per network tick.
struct InputMessage {
    std::uint32_t     tick  = 0;   ///< Client simulation tick at time of send.
    game::PlayerInput input{};     ///< Input state at that tick.
};

/// Encode `msg` into `out`. Always well-formed; never fails.
void serializeInput(net::BitWriter& out, const InputMessage& msg);

/// Decode from `in` into `msg`. Returns false if the stream is malformed
/// or runs out of data before the full message is read.
bool deserializeInput(net::BitReader& in, InputMessage& msg);

}  // namespace engine::replication
