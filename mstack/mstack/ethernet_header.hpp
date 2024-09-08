#pragma once

#include <iomanip>

#include <fmt/format.h>

#include "mac_addr.hpp"
#include "utils.hpp"

namespace mstack {

struct ethernetv2_header_t {
        mac_addr_t dst_mac_addr;
        mac_addr_t src_mac_addr;
        uint16_t   proto = 0;

        static constexpr size_t size() { return mac_addr_t::size() + mac_addr_t::size() + 2; }

        static ethernetv2_header_t consume(std::byte* ptr) {
                ethernetv2_header_t ethernet_header;
                ethernet_header.dst_mac_addr.consume(ptr);
                ethernet_header.src_mac_addr.consume(ptr);
                ethernet_header.proto = utils::consume<uint16_t>(ptr);
                return ethernet_header;
        }

        void produce(std::byte* ptr) {
                dst_mac_addr.produce(ptr);
                src_mac_addr.produce(ptr);
                utils::produce<uint16_t>(ptr, proto);
        }
};

inline std::ostream& operator<<(std::ostream& out, ethernetv2_header_t const& m) {
        out << "[ETHERNET PACKET] ";
        out << "DST: " << mac_addr_t(m.dst_mac_addr) << " SRC: " << mac_addr_t(m.src_mac_addr);
        out << " TYPE: " << std::setiosflags(std::ios::uppercase) << std::hex << int(m.proto);
        return out;
}

}  // namespace mstack

template <>
struct fmt::formatter<mstack::ethernetv2_header_t> : fmt::formatter<std::string> {
        auto format(mstack::ethernetv2_header_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
