#include "ResourceManager.hpp"
#include "Types.hpp"
#include "World.hpp"
#include <vector>

namespace multilife
{

    std::uint64_t ResourceManager::addPlayer(PlayerId playerId) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        
        return m_balances[playerId] = kStartBalance;
    }

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
        auto it = m_balances.find(playerId);
        if (it == m_balances.end() || it->second < amount) {
            return false;
        }
        it->second -= amount;
        return true;
    }

    bool ResourceManager::award(PlayerId playerId, std::uint64_t amount) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_balances.find(playerId);
        if (it == m_balances.end()) {
            return false;
        }
        it->second += amount;
        return true;
    }

    void ResourceManager::awardFromLiveCounts(const std::unordered_map<PlayerId, std::uint64_t>& liveCounts) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& [playerId, live] : liveCounts) {
            m_balances[playerId] += live * kAliveCellAward;
        }
    }

    std::vector<PlayerId> ResourceManager::getPlayerIds() const {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        std::vector<PlayerId> result;
        for (const auto& [playerId, _] : m_balances) {
            result.push_back(playerId);
        }

        return result;
    }

} // namespace multilife

