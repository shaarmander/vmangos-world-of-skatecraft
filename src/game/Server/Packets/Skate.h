// GPL-2.0-or-later. World of Skatecraft protocol v1 (build 5875).
#ifndef MANGOS_PACKETS_SKATE_H
#define MANGOS_PACKETS_SKATE_H
#include "Packet.h"
#include <vector>
constexpr uint16 CMSG_SKATE = 828;
constexpr uint16 SMSG_SKATE = 829;
constexpr uint16 SKATE_OPCODE_COUNT = 830;
class SkatePacket final : public ClientPacket
{
public:
    SkatePacket() : ClientPacket(CMSG_SKATE) {}
    std::vector<uint8> bytes;
    uint8 kind = 255;
    uint32 sequence = 0, map = 0;
    float x = 0, y = 0, z = 0;
    bool valid = false;
    void ReadFromWorldPacket(WorldPacket& packet) override;
};
#endif
