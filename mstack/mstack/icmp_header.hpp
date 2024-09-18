#pragma once

#include <cstdint>

#include <ostream>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include "utils.hpp"

namespace mstack {

struct icmp_header_t {
        uint8_t  proto_type = 0;
        uint8_t  code       = 0;
        uint16_t checksum   = 0;
        uint16_t id         = 0;
        uint16_t seq        = 0;

        static constexpr size_t size() { return 1 + 1 + 2 + 2 + 2; }

        static icmp_header_t consume(std::byte* ptr) {
                return {
                        .proto_type = utils::consume<uint8_t>(ptr),
                        .code       = utils::consume<uint8_t>(ptr),
                        .checksum   = utils::consume<uint16_t>(ptr),
                        .id         = utils::consume<uint16_t>(ptr),
                        .seq        = utils::consume<uint16_t>(ptr),
                };
        }

        std::byte* produce(std::byte* ptr) const {
                utils::produce(ptr, proto_type);
                utils::produce(ptr, code);
                utils::produce(ptr, checksum);
                utils::produce(ptr, id);
                utils::produce(ptr, seq);
                return ptr;
        }

        friend std::ostream& operator<<(std::ostream& out, icmp_header_t const& m) {
                using u = uint32_t;
                return out << std::format("[ICMP PACKET] {} {} {} {}", u(m.proto_type), u(m.code),
                                          u(m.id), u(m.seq));
        };
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::icmp_header_t> : fmt::formatter<std::string> {
        auto format(mstack::icmp_header_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
