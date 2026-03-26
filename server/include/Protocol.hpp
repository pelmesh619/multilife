#pragma once
#include <cstdint>

namespace multilife::proto
{

// magic number
inline constexpr std::uint32_t kMagic = 0x4D4C4946u; // 'MLIF'

// ====== TCP ======

// handshake sent to server is a playerId
// [uint32 magic][uint64 playerId]
inline constexpr std::size_t  kHandshakeSize = 12;

// PlayerCommand
// [uint8 type][uint64 playerId][int64 x][int64 y]
inline constexpr std::uint8_t kCmdPlace = 0x00;
inline constexpr std::uint8_t kCmdRemove = 0x01;
inline constexpr std::uint8_t kCmdToggle = 0x02;
inline constexpr std::size_t  kCommandSize = 25;

// Resync request - sent by client in case of missing sequence numbers
// [uint8 type=0x10]
inline constexpr std::uint8_t kMsgResyncReq = 0x10;
inline constexpr std::size_t  kResyncReqSize = 1;

// ====== UDP ======

// UDP datagram is a chunk update

// Header
//  [uint32 seqNum]   4 bytes
//  [uint8  flags]    1 byte     first bit - 0 = delta, 1 = full snapshot
//  [int32  chunkX]   4 bytes    chunk coords
//  [int32  chunkY]   4 bytes
//  [uint16 cellCount]  2 bytes  number of cell entries

// Cell entry
//   [uint8  localX]   1 byte
//   [uint8  localY]   1 byte
//   [uint8  alive]    1 byte    1 = born/surviving, 0 = died
//   [uint64 owner]    8 bytes   playerId (0 if dead either no owner)

// Packet capped at kMaxUdpPayload, overflow cells silently dropped.

inline constexpr std::size_t  kUdpHeader         = 15;
inline constexpr std::size_t  kUdpCellEntry       = 11;
inline constexpr std::size_t  kMaxUdpPayload      = 1400;
inline constexpr std::size_t  kMaxCellsPerPacket  =
    (kMaxUdpPayload - kUdpHeader) / kUdpCellEntry;

// Offsets within the header
inline constexpr std::size_t  kOffSeqNum    = 0;
inline constexpr std::size_t  kOffFlags     = 4;
inline constexpr std::size_t  kOffChunkX    = 5;
inline constexpr std::size_t  kOffChunkY    = 9;
inline constexpr std::size_t  kOffCellCount = 13;

inline constexpr std::uint8_t kFlagDelta    = 0x00;
inline constexpr std::uint8_t kFlagFull     = 0x01;

} // namespace multilife::proto