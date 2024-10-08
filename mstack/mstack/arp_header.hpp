#pragma once

#include <cstddef>
#include <cstdint>

#include <ostream>
#include <sstream>
#include <string>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "ipv4_addr.hpp"
#include "mac_addr.hpp"
#include "utils.hpp"

namespace mstack {

struct arpv4_header_t {
        uint16_t    htype;
        uint16_t    ptype;
        uint8_t     hlen;
        uint8_t     plen;
        uint16_t    oper;
        mac_addr_t  sha;
        ipv4_addr_t spa;
        mac_addr_t  tha;
        ipv4_addr_t tpa;

        static constexpr size_t size() {
                return 2 + 2 + 1 + 1 + 2 + mac_addr_t::size() * 2 + ipv4_addr_t::size() * 2;
        }

        static arpv4_header_t consume_from_net(std::byte* ptr) {
                arpv4_header_t arpv4_header;
                arpv4_header.htype = utils::consume_from_net<uint16_t>(ptr);
                arpv4_header.ptype = utils::consume_from_net<uint16_t>(ptr);
                arpv4_header.hlen  = utils::consume_from_net<uint8_t>(ptr);
                arpv4_header.plen  = utils::consume_from_net<uint8_t>(ptr);
                arpv4_header.oper  = utils::consume_from_net<uint16_t>(ptr);
                arpv4_header.sha.consume_from_net(ptr);
                arpv4_header.spa.consume_from_net(ptr);
                arpv4_header.tha.consume_from_net(ptr);
                arpv4_header.tpa.consume_from_net(ptr);
                return arpv4_header;
        }

        void produce_to_net(std::byte* ptr) const {
                utils::produce_to_net(ptr, htype);
                utils::produce_to_net(ptr, ptype);
                utils::produce_to_net(ptr, hlen);
                utils::produce_to_net(ptr, plen);
                utils::produce_to_net(ptr, oper);
                this->sha.produce_to_net(ptr);
                this->spa.produce_to_net(ptr);
                this->tha.produce_to_net(ptr);
                this->tpa.produce_to_net(ptr);
        }

        friend std::ostream& operator<<(std::ostream& out, arpv4_header_t const& m) {
                out << "op=" << m.oper;
                out << ", { sha=" << m.sha << ", spa=" << m.spa << " }";
                out << " -> ";
                out << "{ tha=" << m.tha << ", tpa=" << m.tpa << " }";
                return out;
        }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::arpv4_header_t> : fmt::formatter<std::string> {
        auto format(mstack::arpv4_header_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
