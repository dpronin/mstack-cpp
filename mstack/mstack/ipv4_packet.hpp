#pragma once

#include <cstdint>

#include <memory>
#include <ostream>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include "base_packet.hpp"
#include "ipv4_addr.hpp"

namespace mstack {

struct ipv4_packet {
        ipv4_addr_t                  src_ipv4_addr;
        ipv4_addr_t                  dst_ipv4_addr;
        uint16_t                     proto;
        std::unique_ptr<base_packet> buffer;

        friend std::ostream& operator<<(std::ostream& out, ipv4_packet const& p) {
                out << p.src_ipv4_addr;
                out << " -> ";
                out << p.dst_ipv4_addr;
                return out;
        }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::ipv4_packet> : fmt::formatter<std::string> {
        auto format(mstack::ipv4_packet const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
