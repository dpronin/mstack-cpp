#pragma once

#include <cstddef>
#include <cstdint>

#include <ostream>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include "utils.hpp"

namespace mstack {

struct icmp_header_t {
        uint8_t  proto_type;
        uint8_t  code;
        uint16_t chsum;
        uint16_t id;
        uint16_t seq;

        static constexpr size_t size() { return sizeof(icmp_header_t); }

        static icmp_header_t consume_from_net(std::byte* ptr) {
                return {
                        .proto_type = utils::consume_from_net<uint8_t>(ptr),
                        .code       = utils::consume_from_net<uint8_t>(ptr),
                        .chsum      = utils::consume_from_net<uint16_t>(ptr),
                        .id         = utils::consume_from_net<uint16_t>(ptr),
                        .seq        = utils::consume_from_net<uint16_t>(ptr),
                };
        }

        std::byte* produce_to_net(std::byte* ptr) const {
                utils::produce_to_net(ptr, proto_type);
                utils::produce_to_net(ptr, code);
                utils::produce_to_net(ptr, chsum);
                utils::produce_to_net(ptr, id);
                utils::produce_to_net(ptr, seq);
                return ptr;
        }

        friend std::ostream& operator<<(std::ostream& out, icmp_header_t const& m) {
                using u = uint32_t;
                return out << std::format("[ICMP PACKET] {} {} {} {}", u(m.proto_type), u(m.code),
                                          u(m.id), u(m.seq));
        }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::icmp_header_t> : fmt::formatter<std::string> {
        auto format(mstack::icmp_header_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
