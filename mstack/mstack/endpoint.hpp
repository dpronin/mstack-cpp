#pragma once

#include <cstdint>

#include <functional>
#include <ostream>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include <boost/container_hash/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include "ipv4_addr.hpp"

namespace mstack {

using port_addr_t = uint16_t;

struct endpoint {
        ipv4_addr_t addrv4;
        port_addr_t addrv4_port;

        ipv4_addr_t address() const { return addrv4; }
        port_addr_t port() const { return addrv4_port; }

        auto operator<=>(const endpoint& rhs) const = default;

        friend std::ostream& operator<<(std::ostream& out, endpoint const& p) {
                out << p.addrv4;
                out << ":";
                out << p.addrv4_port;
                return out;
        }

        static endpoint make_from(std::string_view host, std::string_view port) {
                return {
                        mstack::ipv4_addr_t::make_from(host),
                        boost::numeric_cast<uint16_t>(boost::lexical_cast<int64_t>(port)),
                };
        }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::endpoint> : fmt::formatter<std::string> {
        auto format(mstack::endpoint const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};

namespace std {

template <>
struct hash<mstack::endpoint> {
        size_t operator()(const mstack::endpoint& ep) const {
                size_t seed{0};
                boost::hash_combine(seed, ep.addrv4);
                boost::hash_combine(seed, ep.addrv4_port);
                return seed;
        };
};

}  // namespace std

namespace mstack {
inline size_t hash_value(endpoint const& v) { return std::hash<endpoint>{}(v); }
}  // namespace mstack
