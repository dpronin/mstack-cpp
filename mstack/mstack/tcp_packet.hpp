#pragma once

#include <cstdint>

#include <memory>
#include <ostream>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include "ipv4_port.hpp"
#include "skbuff.hpp"

namespace mstack {

struct tcp_packet {
        uint8_t     proto;
        ipv4_port_t remote_info;
        ipv4_port_t local_info;
        skbuff      skb;

        friend std::ostream& operator<<(std::ostream& out, tcp_packet const& p) {
                out << p.remote_info;
                out << " -> ";
                out << p.local_info;
                return out;
        }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::tcp_packet> : fmt::formatter<std::string> {
        auto format(mstack::tcp_packet const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
