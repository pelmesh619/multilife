#pragma once

#include "Types.hpp"

#include <cstdint>

namespace multilife
{

    enum class CommandType : std::uint8_t {
        PlaceCell,
        RemoveCell,
        ToggleCell,
    };

    struct PlayerCommand {
        PlayerId playerId{};
        CommandType type{CommandType::PlaceCell};

        std::int64_t x{0};
        std::int64_t y{0};
    };

} // namespace multilife

