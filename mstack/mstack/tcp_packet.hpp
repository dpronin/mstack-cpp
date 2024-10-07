#pragma once

#include <cstdint>

#include <ostream>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include "endpoint.hpp"
#include "skbuff.hpp"

namespace mstack {

struct tcp_packet {
        uint8_t  proto;
        endpoint remote_ep;
        endpoint local_ep;
        skbuff   skb;

        friend std::ostream& operator<<(std::ostream& out, tcp_packet const& p) {
                out << p.remote_ep;
                out << " -> ";
                out << p.local_ep;
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
