#pragma once

#include <optional>
#include <unordered_map>
#include <utility>

#include "ipv4_addr.hpp"
#include "mac_addr.hpp"

#include <spdlog/spdlog.h>

namespace mstack {

class arp_cache_t {
public:
        void reset(ipv4_addr_t const& ipv4_addr) {
                if (auto it{cache_.find(ipv4_addr)}; cache_.end() != it) {
                        spdlog::debug("[REMOVE ARP CACHE] {}:{}", it->first, it->second);
                        cache_.erase(it);
                }
        }

        void update(ipv4_addr_t const& ipv4_addr, mac_addr_t const& mac_addr) {
                spdlog::debug("[UPDATE ARP CACHE] {} -> {}", ipv4_addr, mac_addr);
                cache_[ipv4_addr] = mac_addr;
        }

        void update(std::pair<ipv4_addr_t, mac_addr_t> const& kv) { update(kv.first, kv.second); }

        std::optional<mac_addr_t> query(ipv4_addr_t const& ipv4_addr) {
                if (auto it{cache_.find(ipv4_addr)}; cache_.end() != it) {
                        return it->second;
                }
                return std::nullopt;
        }

private:
        std::unordered_map<ipv4_addr_t, mac_addr_t> cache_;
};

}  // namespace mstack
