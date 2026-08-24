#pragma once

#include <cstddef>
#include <cstdint>

namespace multilife::client::proto {

inline constexpr std::uint32_t kMagic = 0x4D4C4946u; // 'MLIF'

inline constexpr std::uint8_t kCmdPlace = 0x00;
inline constexpr std::uint8_t kCmdRemove = 0x01;
inline constexpr std::uint8_t kCmdToggle = 0x02;
inline constexpr std::uint8_t kMsgResyncReq = 0x10;
inline constexpr std::uint8_t kMsgServerStats = 0x20;

inline constexpr std::size_t kHandshakeSize = 12;
inline constexpr std::size_t kCommandSize = 25;
inline constexpr std::size_t kServerStatsHeader = 7;
inline constexpr std::size_t kServerStatsEntry = 24;

inline constexpr std::size_t kUdpHeader = 15;
inline constexpr std::size_t kUdpCellEntry = 11;
inline constexpr std::size_t kMaxUdpPayload = 1400;

inline constexpr std::size_t kOffSeqNum = 0;
inline constexpr std::size_t kOffFlags = 4;
inline constexpr std::size_t kOffChunkX = 5;
inline constexpr std::size_t kOffChunkY = 9;
inline constexpr std::size_t kOffCellCount = 13;

inline constexpr std::uint8_t kFlagDelta = 0x00;
inline constexpr std::uint8_t kFlagFull = 0x01;

inline constexpr std::int32_t kChunkWidth = 64;
inline constexpr std::int32_t kChunkHeight = 64;

} // namespace multilife::client::proto
