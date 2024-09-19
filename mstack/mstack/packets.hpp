#pragma once

#include <cstdint>

#include <memory>
#include <string>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

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
                return hash<mstack::ipv4_port_t>{}(two_ends.remote_info) ^
                       hash<mstack::ipv4_port_t>{}(two_ends.local_info);
        }
};

}  // namespace std

template <>
struct fmt::formatter<mstack::two_ends_t> : fmt::formatter<std::string> {
        auto format(mstack::two_ends_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
