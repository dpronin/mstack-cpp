#pragma once

#include <cstdint>

#include <memory>
#include <string>

#include <fmt/format.h>

#include <spdlog/spdlog.h>

#include <boost/container_hash/hash.hpp>

#include "base_packet.hpp"
#include "ipv4_port.hpp"

namespace mstack {

struct two_ends_t {
        ipv4_port_t remote_info;
        ipv4_port_t local_info;

        auto operator<=>(two_ends_t const& other) const = default;

        friend std::ostream& operator<<(std::ostream& out, two_ends_t const& p) {
                out << p.remote_info;
                out << " -> ";
                out << p.local_info;
                return out;
        }
};

struct tcp_packet_t {
        uint16_t                     proto;
        ipv4_port_t                  remote_info;
        ipv4_port_t                  local_info;
        std::unique_ptr<base_packet> buffer;
};

};  // namespace mstack

namespace std {

template <>
struct hash<mstack::two_ends_t> {
        size_t operator()(const mstack::two_ends_t& two_ends) const {
                size_t seed{0zu};
                boost::hash_combine(seed, two_ends.remote_info);
                boost::hash_combine(seed, two_ends.local_info);
                return seed;
        }
};

}  // namespace std

namespace mstack {
inline size_t hash_value(two_ends_t const& v) { return std::hash<two_ends_t>{}(v); }
}  // namespace mstack

template <>
struct fmt::formatter<mstack::two_ends_t> : fmt::formatter<std::string> {
        auto format(mstack::two_ends_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
