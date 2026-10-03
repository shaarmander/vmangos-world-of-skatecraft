# vmangos-world-of-skatecraft

VMaNGOS with a negotiated multiplayer skating extension for the World of Skatecraft
custom client. Based on upstream `4b3d241cffe245a1f68d` (build 5875).
WoW still owns authentication, characters, maps, instances, visibility and gameplay.
Each skating client simulates its own Skate engine and publishes the solved skeleton.
The server authenticates the sender and relays it only to visible, opted-in peers.

This is free-skate replication. It does not validate tricks or scores, simulate other
players' boards, or provide authoritative competitive physics. Stock WoW clients see
ordinary player movement and receive no skating extension packets.

## Build and run

```
docker build -f Dockerfile.skate -t vmangos-world-of-skatecraft:5875 .
```

Use this image for the `mangosd` service in vmangos-deploy. Retain its original
`realmd`, database and configuration. No SQL migrations are introduced.
Extracted navigation data must match this core; see below before reusing another
server's data files.
The companion client patch is `client/world-of-skatecraft.patch`, generated against
[World of Skatecraft](https://github.com/Kimmo3223/world-of-skatecraft) commit
`da6e99bc`. In an existing working World of Skatecraft installation, apply it with
`git apply ../vmangos-world-of-skatecraft/client/world-of-skatecraft.patch`, rebuild
with `cargo build --release -p benilla`, and set `WOW_SKATE_MULTIPLAYER=1`.
The patch contains source only; obtain/convert your own WoW and Skate assets through
the original project's setup. Test launchers expect locally provisioned accounts;
no account credentials or game assets are included. Press J to ride; an Xbox-style controller drives the engine.
Each simultaneous client needs its own account and character.

## Wire v1

New opcodes: CMSG_SKATE = 828 (0x33c), SMSG_SKATE = 829 (0x33d).
All numbers are little endian. The world connection supplies authentication and
ordering. The server prefixes each response with an **unpacked u64 player GUID**;
only the hello acknowledgment uses GUID zero. A client never supplies that GUID.

Every body starts with u8 version (=1), u8 kind, u32 sequence, u32 map ID.
Kind 0 is HELLO/ACK, kind 1 is a pose, kind 2 ends skating.
HELLO and STOP have no additional fields. A client waits for ACK before publishing.

POSE adds:

* Origin: three f32 values, Bevy world yards: (-WoW y, WoW z, -WoW x).
* Root transform, engine meters from the origin.
* u16 bone count, then named root-relative bone transforms in engine meters.
* Each bone is a u8 name length, ASCII uppercase/digit/underscore name bytes,
  and a transform. Board, trucks and wheel joints use the same representation.
* Each transform is ten f32 values: translation xyz, quaternion xyzw, scale xyz.

Bounds: 10,000 body bytes, 1–96 unique bones, names 1–48 bytes, finite values,
near-unit quaternions, positive bounded scales. Trailing/truncated payloads are
invalid. Sequence numbers increase for each character session. Frames are sent at
20 Hz; the server limits accepted poses to at most 40 Hz. No pose is cached server
side; newly visible peers receive the next periodic frame.

The client buffers eight poses and interpolates positions/scales and quaternion
rotations at a 100 ms delay. It retargets the skeleton to each peer's WoW race/model
and skins an independent board. STOP, entity removal and a one-second stale timeout
remove the remote pose/board; teleports, logout and reconnect clear client state.

## Movement integration

Only an authenticated, negotiated, living, unmounted, unrooted, unstunned player
with a fresh accepted pose receives the skating position-test exemption. Normal
flag validation remains active. Boarding must begin near the WoW player position;
subsequent pose displacements are bounded, and ordinary movement must remain near
the latest skating pose. Death, transport/taxi use, teleport, stopping or timeout
ends the exemption. These bounds limit malformed movement; they are not a claim
of cheat-proof skating.

The custom client also sends normal WoW movement at 20 Hz while skating, including
when horizontal speed is zero. This keeps spatial visibility and WoW gameplay
position current. Ordinary walking retains the stock movement cadence.

## Creature navigation and clean client data

This core requires movement-map generator version 6, Detour version 7, collision
maps `VMAP_7.0`, and terrain maps `MAPSz1.4`. CMaNGOS generator-v8 movement tiles
are rejected even when their filenames look correct. Do not rewrite their version
headers or disable pathfinding to hide the mismatch.

Build with `-DBUILD_EXTRACTORS=ON` and install the extractors. In a fresh output
directory, run `tools/extract-navigation.sh` with `CLIENT` pointing to a clean
user-owned WoW 1.12.1 installation and `EXTRACTORS` pointing to the installed
`Extractors` directory (including config.json and offmesh.txt). All output can live
on a separate drive. Use the same clean MPQ chain for the custom client; a World
Forge patch that replaces an ADT changes what the client sees but is not included
in the stock server extractor's patch list.

Before deployment, run `python tools/check-navigation.py <output-directory>
--require-tile 0004832` (Northshire). Back up the active extracted-data directory
and stop the world server before replacing it. Retain `mmap.enabled = 1` and all
three `vmap.enable*` settings. Restart and verify `.mmap loc` resolves a polygon,
`.mmap stats` reports loaded tiles, and a creature can chase a normal, non-GM-mode
character. Header checks alone do not establish correct paths or rendered motion.
