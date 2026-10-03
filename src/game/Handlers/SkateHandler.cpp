// GPL-2.0-or-later. Client-owned skating pose relay; no competitive score authority.
#include "WorldSession.h"
#include "Player.h"
#include "Map.h"
#include "Timer.h"
#include <cmath>
#include <set>

void SkatePacket::ReadFromWorldPacket(WorldPacket& p)
{
    // Bound before allocating. Malformed frames are consumed but never relayed.
    if (p.size() < 10 || p.size() > 10000) { p.rpos(p.size()); return; }
    bytes.assign(p.contents(), p.contents() + p.size());
    uint8 version;
    p >> version >> kind >> sequence >> map;
    if (version != 1 || kind > 2) { p.rpos(p.size()); return; }
    if (kind != 1) { valid = p.rpos() == p.size(); return; }
    float origin[3];
    for (float& v : origin) { p >> v; if (!std::isfinite(v) || std::abs(v) > 50000) return; }
    // Transform is translation, quaternion (xyzw), scale. Bones are root-relative meters.
    auto transform = [&p](float* position, float bound) {
        float qnorm = 0;
        for (int i = 0; i < 10; ++i) {
            float v; p >> v;
            if (!std::isfinite(v)) return false;
            if (i < 3) { position[i] = v; if (std::abs(v) > bound) return false; }
            else if (i < 7) qnorm += v * v;
            else if (v < 0.01f || v > 10.0f) return false;
        }
        return qnorm > 0.98f && qnorm < 1.02f;
    };
    float root[3];
    if (!transform(root, 100000)) return;
    x = -(origin[2] + root[2] / 0.9144f);
    y = -(origin[0] + root[0] / 0.9144f);
    z = origin[1] + root[1] / 0.9144f;
    uint16 count; p >> count;
    if (!count || count > 96) return;
    std::set<std::string> names;
    for (uint16 i = 0; i < count; ++i) {
        uint8 length; p >> length;
        if (!length || length > 48) return;
        std::string name;
        for (uint8 j = 0; j < length; ++j) {
            uint8 c; p >> c;
            if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return;
            name += char(c);
        }
        if (!names.insert(name).second) return;
        float pos[3];
        if (!transform(pos, 100)) return;
    }
    valid = p.rpos() == p.size();
}

bool WorldSession::IsSkating() const
{
    return m_skateActive && WorldTimer::getMSTimeDiff(m_skateTime, WorldTimer::getMSTime()) < 1000 &&
        _player && _player->GetConfirmedMover() == _player && _player->IsAlive() && !_player->IsBeingTeleported() &&
        !_player->IsMounted() && !_player->IsTaxiFlying() && !_player->GetTransport() &&
        !_player->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED);
}

void WorldSession::HandleSkate(SkatePacket const& p)
{
    if (!p.valid) return;
    if (p.kind == 0) {
        m_skateEnabled = true;
        m_skateActive = false;
        m_skateSequence = 0;
        WorldPacket ack(SMSG_SKATE, 18);
        ack << uint64(0);
        ack.append(p.bytes.data(), p.bytes.size());
        SendPacket(&ack);
        return;
    }
    if (!m_skateEnabled || p.map != _player->GetMapId() || p.sequence <= m_skateSequence) return;
    uint32 now = WorldTimer::getMSTime();
    uint32 elapsed = WorldTimer::getMSTimeDiff(m_skateTime, now);
    if (p.kind == 1) {
        if (elapsed < 25 || _player->GetConfirmedMover() != _player || !_player->IsAlive() || _player->IsBeingTeleported() ||
            _player->IsMounted() || _player->IsTaxiFlying() || _player->GetTransport() ||
            _player->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED)) return;
        // Keep the cosmetic pose near the actual WoW mover. The movement handler still
        // enforces its own flags, teleports and root state; only speed/flying tests differ.
        if (_player->GetDistance(p.x, p.y, p.z) > (IsSkating() ? 80.0f : 8.0f)) return;
        if (IsSkating()) {
            float dx = p.x-m_skateX, dy = p.y-m_skateY, dz = p.z-m_skateZ;
            float maximum = 3.0f + 150.0f * std::min(elapsed, 1000u)/1000.0f;
            if (dx*dx + dy*dy + dz*dz > maximum*maximum) return;
        }
        m_skateX = p.x; m_skateY = p.y; m_skateZ = p.z;
        m_skateActive = true;
        m_skateTime = now;
    } else m_skateActive = false;
    m_skateSequence = p.sequence;
    WorldPacket relay(SMSG_SKATE, p.bytes.size() + 8);
    relay << _player->GetObjectGuid().GetRawValue();
    relay.append(p.bytes.data(), p.bytes.size());
    for (auto const& ref : _player->GetMap()->GetPlayers()) {
        Player* peer = ref.getSource();
        if (peer != _player && peer->GetSession()->m_skateEnabled && peer->IsInVisibleList(_player))
            peer->GetSession()->SendPacket(&relay);
    }
}
