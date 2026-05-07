#include "engine/replication/InputMessage.h"

namespace engine::replication {

void serializeInput(net::BitWriter& out, const InputMessage& msg) {
    out.writeVarint(msg.tick);
    out.writeBool(msg.input.moveUp);
    out.writeBool(msg.input.moveDown);
    out.writeBool(msg.input.moveLeft);
    out.writeBool(msg.input.moveRight);
    out.writeBool(msg.input.attack);
    out.writeBool(msg.input.dodge);
}

bool deserializeInput(net::BitReader& in, InputMessage& msg) {
    msg.tick            = in.readVarint();
    msg.input.moveUp    = in.readBool();
    msg.input.moveDown  = in.readBool();
    msg.input.moveLeft  = in.readBool();
    msg.input.moveRight = in.readBool();
    msg.input.attack    = in.readBool();
    msg.input.dodge     = in.readBool();
    return in.ok();
}

}  // namespace engine::replication
