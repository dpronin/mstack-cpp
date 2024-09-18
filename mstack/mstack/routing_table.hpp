#pragma once

#include <optional>
#include <unordered_map>
#include <utility>

#include <spdlog/spdlog.h>

#include "ipv4_addr.hpp"

#include <spdlog/spdlog.h>

namespace mstack {

class routing_table {
public:
        void reset(ipv4_addr_t const& addr) {
                if (auto it{cache_.find(addr)}; cache_.end() != it) {
                        spdlog::debug("[RT TBL] rem: {} via {}", it->first, it->second);
                        cache_.erase(it);
                }
        }

        void update(std::pair<ipv4_addr_t, ipv4_addr_t> const& kv) {
                spdlog::debug("[RT TBL] upd: {} via {}", kv.first, kv.second);
                cache_.insert(kv);
        }

        std::optional<ipv4_addr_t> query(ipv4_addr_t const& addr) {
                if (auto it{cache_.find(addr)}; cache_.end() != it) {
                        return it->second;
                }
                return std::nullopt;
        }

private:
        std::unordered_map<ipv4_addr_t, ipv4_addr_t> cache_;
};

}  // namespace mstack
