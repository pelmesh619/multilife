#pragma once

#include "Types.hpp"

#include <unordered_map>
#include <shared_mutex>
#include <mutex>

namespace multilife
{

    class ResourceManager
    {
    public:
        ResourceManager() = default;

        std::uint64_t addPlayer(PlayerId playerId);

        std::uint64_t getBalance(PlayerId playerId) const;

        bool trySpend(PlayerId playerId, std::uint64_t amount);

        bool award(PlayerId playerId, std::uint64_t amount);

        void awardFromLiveCounts(const std::unordered_map<PlayerId, std::uint64_t>& liveCounts);

        std::vector<PlayerId> getPlayerIds() const;

    private:
        mutable std::shared_mutex m_mutex;
        std::unordered_map<PlayerId, std::uint64_t> m_balances;
    };

} // namespace multilife

