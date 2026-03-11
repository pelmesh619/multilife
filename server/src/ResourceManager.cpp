#include "ResourceManager.hpp"

namespace multilife
{

    std::uint64_t ResourceManager::getBalance(PlayerId playerId) const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_balances.find(playerId);
        if (it == m_balances.end()) {
            return 0;
        }
        return it->second;
    }

    bool ResourceManager::trySpend(PlayerId playerId, std::uint64_t amount) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto& balance = m_balances[playerId];
        if (balance < amount) {
            return false;
        }
        balance -= amount;
        return true;
    }

    void ResourceManager::awardFromLiveCounts(const std::unordered_map<PlayerId, std::uint64_t>& liveCounts) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& [playerId, live] : liveCounts) {
            m_balances[playerId] += live;
        }
    }

} // namespace multilife

