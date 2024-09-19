#pragma once

#include <cstdint>

#include <functional>
#include <ostream>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include <boost/container_hash/hash.hpp>

#include "ipv4_addr.hpp"

namespace mstack {

using port_addr_t = uint16_t;

struct ipv4_port_t {
        ipv4_addr_t ipv4_addr;
        port_addr_t port_addr;

        auto operator<=>(const ipv4_port_t& rhs) const = default;

        friend std::ostream& operator<<(std::ostream& out, ipv4_port_t const& p) {
                out << p.ipv4_addr;
                out << ":";
                out << p.port_addr;
                return out;
        }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::ipv4_port_t> : fmt::formatter<std::string> {
        auto format(mstack::ipv4_port_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};

namespace std {

template <>
struct hash<mstack::ipv4_port_t> {
        size_t operator()(const mstack::ipv4_port_t& ipv4_port) const {
                auto seed{0zu};
                boost::hash_combine(seed, ipv4_port.ipv4_addr);
                boost::hash_combine(seed, ipv4_port.port_addr);
                return seed;
        };
};

}  // namespace std

namespace mstack {
inline size_t hash_value(ipv4_port_t const& v) { return std::hash<ipv4_port_t>{}(v); }
}  // namespace mstack
