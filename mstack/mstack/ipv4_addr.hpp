#pragma once

#include <cstddef>
#include <cstdint>

#include <format>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>

#include <boost/asio/ip/address_v4.hpp>
#include <boost/container_hash/hash.hpp>

#include <fmt/format.h>

#include "utils.hpp"

namespace mstack {

class ipv4_addr_t {
private:
        uint32_t raw_ = 0;

public:
        ipv4_addr_t() = default;
        ipv4_addr_t(uint32_t raw) : raw_(raw) {}

        auto operator<=>(const ipv4_addr_t& other) const = default;

        uint32_t raw() const { return raw_; }

        static constexpr size_t size() { return 4; }

        void consume_from_net(std::byte*& ptr) { raw_ = utils::consume_from_net<uint32_t>(ptr); }

        void produce_to_net(std::byte*& ptr) const { utils::produce_to_net(ptr, raw_); }

        friend std::ostream& operator<<(std::ostream& out, ipv4_addr_t ipv4) {
                out << std::format("{}.{}.{}.{}", (ipv4.raw_ >> 24) & 0xFF,
                                   (ipv4.raw_ >> 16) & 0xFF, (ipv4.raw_ >> 8) & 0xFF,
                                   (ipv4.raw_ >> 0) & 0xFF);
                return out;
        }

        static ipv4_addr_t make_from(std::string_view source) {
                return {boost::asio::ip::make_address_v4(source).to_uint()};
        }

        std::string to_string() const { return (std::ostringstream{} << *this).str(); }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::ipv4_addr_t> : fmt::formatter<std::string> {
        auto format(mstack::ipv4_addr_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};

namespace std {
template <>
struct hash<mstack::ipv4_addr_t> {
        size_t operator()(const mstack::ipv4_addr_t& ipv4_addr) const {
                return hash<uint32_t>{}(ipv4_addr.raw());
        }
};

}  // namespace std

namespace mstack {
inline size_t hash_value(ipv4_addr_t const& v) { return std::hash<ipv4_addr_t>{}(v); }
}  // namespace mstack
