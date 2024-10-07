#pragma once

#include <cstdint>

#include <ostream>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include "mac_addr.hpp"
#include "utils.hpp"

namespace mstack {

struct ethernetv2_header_t {
        mac_addr_t dst_mac_addr;
        mac_addr_t src_mac_addr;
        uint16_t   proto = 0;

        static constexpr size_t size() { return 2 * mac_addr_t::size() + 2; }

        static ethernetv2_header_t consume_from_net(std::byte* ptr) {
                ethernetv2_header_t ethernet_header;
                ethernet_header.dst_mac_addr.consume_from_net(ptr);
                ethernet_header.src_mac_addr.consume_from_net(ptr);
                ethernet_header.proto = utils::consume_from_net<uint16_t>(ptr);
                return ethernet_header;
        }

        void produce_to_net(std::byte* ptr) const {
                dst_mac_addr.produce_to_net(ptr);
                src_mac_addr.produce_to_net(ptr);
                utils::produce_to_net(ptr, proto);
        }
};

inline std::ostream& operator<<(std::ostream& out, ethernetv2_header_t const& m) {
        out << fmt::format("[ETH PACKET] DST: {} SRC: {} TYPE: {:04X}", m.dst_mac_addr,
                           m.src_mac_addr, m.proto);
        return out;
}

}  // namespace mstack

template <>
struct fmt::formatter<mstack::ethernetv2_header_t> : fmt::formatter<std::string> {
        auto format(mstack::ethernetv2_header_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
