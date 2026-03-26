#pragma once
#include <cstdint>

namespace multilife::proto
{

// ── Magic / framing ───────────────────────────────────────────────────────────
inline constexpr std::uint32_t kMagic = 0x4D4C4946u; // 'MLIF'

// ── TCP: client → server messages ────────────────────────────────────────────

// Handshake  [uint32 magic][uint64 playerId]                    12 bytes
inline constexpr std::size_t  kHandshakeSize  = 12;

// PlayerCommand  [uint8 type][uint64 playerId][int64 x][int64 y]  25 bytes
inline constexpr std::uint8_t kCmdPlace       = 0x00;
inline constexpr std::uint8_t kCmdRemove      = 0x01;
inline constexpr std::uint8_t kCmdToggle      = 0x02;
inline constexpr std::size_t  kCommandSize    = 25;

// Resync request  [uint8 type=0x10]                               1 byte
// Sent by client when it detects a gap in sequence numbers.
// Server responds with a full-snapshot broadcast for that session.
inline constexpr std::uint8_t kMsgResyncReq   = 0x10;
inline constexpr std::size_t  kResyncReqSize  = 1;

// ── UDP: server → client messages ────────────────────────────────────────────
//
// Every UDP datagram carries exactly one chunk update.
//
// Header (15 bytes):
//   [uint32 seqNum  ]   4 bytes   monotonically increasing per broadcast tick
//   [uint8  flags   ]   1 byte    bit 0: 0 = delta, 1 = full snapshot
//   [int32  chunkX  ]   4 bytes
//   [int32  chunkY  ]   4 bytes
//   [uint16 cellCount]  2 bytes   number of cell entries that follow
//
// Per cell entry (11 bytes):
//   [uint8  localX  ]   1 byte
//   [uint8  localY  ]   1 byte
//   [uint8  alive   ]   1 byte    1 = born/surviving, 0 = died
//   [uint64 owner   ]   8 bytes   PlayerId (0 if dead)
//
// Packet capped at kMaxUdpPayload; overflow cells silently dropped.

inline constexpr std::size_t  kUdpHeader         = 15;
inline constexpr std::size_t  kUdpCellEntry       = 11;
inline constexpr std::size_t  kMaxUdpPayload      = 1400;
inline constexpr std::size_t  kMaxCellsPerPacket  =
    (kMaxUdpPayload - kUdpHeader) / kUdpCellEntry;  // 126

// Offsets within the UDP header
inline constexpr std::size_t  kOffSeqNum    = 0;   // uint32
inline constexpr std::size_t  kOffFlags     = 4;   // uint8
inline constexpr std::size_t  kOffChunkX    = 5;   // int32
inline constexpr std::size_t  kOffChunkY    = 9;   // int32
inline constexpr std::size_t  kOffCellCount = 13;  // uint16

inline constexpr std::uint8_t kFlagDelta    = 0x00;
inline constexpr std::uint8_t kFlagFull     = 0x01;

} // namespace multilife::proto