#pragma once

#include <cstdint>

#include <memory>
#include <ostream>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include "base_packet.hpp"
#include "mac_addr.hpp"

namespace mstack {

struct ethernetv2_frame {
        mac_addr_t                   src_mac_addr;
        mac_addr_t                   dst_mac_addr;
        uint16_t                     proto;
        std::unique_ptr<base_packet> buffer;

        friend std::ostream& operator<<(std::ostream& out, ethernetv2_frame const& p) {
                out << p.src_mac_addr;
                out << " -> ";
                out << p.dst_mac_addr;
                return out;
        }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::ethernetv2_frame> : fmt::formatter<std::string> {
        auto format(mstack::ethernetv2_frame const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
