#pragma once

#include <cstdint>

#include <functional>
#include <memory>
#include <optional>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "base_packet.hpp"
#include "ipv4_port.hpp"

namespace mstack {

struct nop_packet {
        uint16_t proto;
};

struct two_ends_t {
        std::optional<ipv4_port_t> remote_info;
        std::optional<ipv4_port_t> local_info;

        bool operator==(const two_ends_t& rhs) const {
                if (!remote_info || !local_info) {
                        spdlog::critical("EMPTY IPV4 PORT");
                }
                return remote_info == rhs.remote_info.value() &&
                       local_info == rhs.local_info.value();
        };

        friend std::ostream& operator<<(std::ostream& out, two_ends_t const& p) {
                if (p.remote_info) {
                        out << p.remote_info.value();
                } else {
                        out << "NONE";
                }

                out << " -> ";

                if (p.local_info) {
                        out << p.local_info.value();
                } else {
                        out << "NONE";
                }

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
                if (!two_ends.remote_info || !two_ends.local_info) {
                        spdlog::critical("EMPTY INFO");
                }
                return hash<mstack::ipv4_port_t>{}(two_ends.remote_info.value()) ^
                       hash<mstack::ipv4_port_t>{}(two_ends.local_info.value());
        }
};

}  // namespace std

template <>
struct fmt::formatter<mstack::two_ends_t> : fmt::formatter<std::string> {
        auto format(mstack::two_ends_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
