#pragma once

#include <optional>
#include <unordered_map>
#include <utility>

#include "ipv4_addr.hpp"
#include "mac_addr.hpp"

#include <spdlog/spdlog.h>

namespace mstack {

class neigh_cache {
public:
        void reset(ipv4_addr_t const& addr) {
                if (auto it{cache_.find(addr)}; cache_.end() != it) {
                        spdlog::debug("[NEIGH] del: {} -> {}", it->first, it->second);
                        cache_.erase(it);
                }
        }

        void update(std::pair<ipv4_addr_t, mac_addr_t> const& kv) {
                spdlog::debug("[NEIGH] upd: {} -> {}", kv.first, kv.second);
                cache_.insert(kv);
        }

        std::optional<mac_addr_t> query(ipv4_addr_t const& addr) const {
                if (auto it{cache_.find(addr)}; cache_.end() != it) {
                        return it->second;
                }
                return std::nullopt;
        }

private:
        std::unordered_map<ipv4_addr_t, mac_addr_t> cache_;
};

}  // namespace mstack
