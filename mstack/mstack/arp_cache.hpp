#pragma once

#include <optional>
#include <unordered_map>

#include "ipv4_addr.hpp"
#include "mac_addr.hpp"

#include <spdlog/spdlog.h>

namespace mstack {

struct arp_cache_t {
        std::unordered_map<int, mac_addr_t>         mac_addr_map;
        std::unordered_map<int, ipv4_addr_t>        ipv4_addr_map;
        std::unordered_map<ipv4_addr_t, mac_addr_t> arp_cache;

        void reset(ipv4_addr_t const& ipv4_addr) {
                if (auto it{arp_cache.find(ipv4_addr)}; arp_cache.end() != it) {
                        spdlog::debug("[REMOVE ARP CACHE] {}:{}", it->first, it->second);
                        arp_cache.erase(it);
                }
        }

        void update(ipv4_addr_t const& ipv4_addr, mac_addr_t const& mac_addr) {
                spdlog::debug("[ADD ARP CACHE] {}:{}", ipv4_addr, mac_addr);
                arp_cache[ipv4_addr] = mac_addr;
        }

        void update(std::pair<ipv4_addr_t, mac_addr_t> const& kv) { update(kv.first, kv.second); }

        std::optional<mac_addr_t> query(ipv4_addr_t const& ipv4_addr) {
                if (arp_cache.find(ipv4_addr) == arp_cache.end()) {
                        return std::nullopt;
                }
                return arp_cache[ipv4_addr];
        }

        std::optional<mac_addr_t> query_dev_mac_addr(int tag) {
                if (mac_addr_map.find(tag) == mac_addr_map.end()) {
                        spdlog::error("[UNKONWN DEV MAC]");
                        return std::nullopt;
                }
                return mac_addr_map[tag];
        }

        std::optional<ipv4_addr_t> query_dev_ipv4_addr(int tag) {
                if (ipv4_addr_map.find(tag) == ipv4_addr_map.end()) {
                        spdlog::error("[UNKONWN DEV IPV4]");
                        return std::nullopt;
                }
                return ipv4_addr_map[tag];
        }
};

}  // namespace mstack
