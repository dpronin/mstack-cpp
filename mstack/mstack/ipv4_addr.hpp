#pragma once

#include <functional>
#include <sstream>
#include <string>

#include <boost/asio/ip/address_v4.hpp>

#include "utils.hpp"

namespace mstack {

class ipv4_addr_t {
private:
        uint32_t v_ = 0;

public:
        ipv4_addr_t()                              = default;
        ~ipv4_addr_t()                             = default;
        ipv4_addr_t(const ipv4_addr_t&)            = default;
        ipv4_addr_t(ipv4_addr_t&&)                 = default;
        ipv4_addr_t& operator=(const ipv4_addr_t&) = default;
        ipv4_addr_t& operator=(ipv4_addr_t&&)      = default;
        ipv4_addr_t(uint32_t ipv4) : v_(ipv4) {}
        ipv4_addr_t(std::string_view ipv4) : v_{boost::asio::ip::make_address_v4(ipv4).to_uint()} {}

        auto operator<=>(const ipv4_addr_t& other) const = default;

        uint32_t raw() const { return v_; }

        static constexpr size_t size() { return 4; }

        void consume(std::byte*& ptr) { v_ = utils::consume<uint32_t>(ptr); }

        void produce(std::byte*& ptr) const { utils::produce<uint32_t>(ptr, v_); }

        friend std::ostream& operator<<(std::ostream& out, ipv4_addr_t ipv4) {
                out << utils::format("{}.{}.{}.{}", (ipv4.v_ >> 24) & 0xFF, (ipv4.v_ >> 16) & 0xFF,
                                     (ipv4.v_ >> 8) & 0xFF, (ipv4.v_ >> 0) & 0xFF);
                return out;
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
