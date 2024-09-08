#pragma once

#include <functional>
#include <sstream>
#include <string>

#include <boost/asio/ip/address_v4.hpp>

#include "utils.hpp"

namespace mstack {

class ipv4_addr_t {
private:
        uint32_t _ipv4 = 0;

public:
        ipv4_addr_t()                              = default;
        ~ipv4_addr_t()                             = default;
        ipv4_addr_t(const ipv4_addr_t&)            = default;
        ipv4_addr_t(ipv4_addr_t&&)                 = default;
        ipv4_addr_t& operator=(const ipv4_addr_t&) = default;
        ipv4_addr_t& operator=(ipv4_addr_t&&)      = default;
        ipv4_addr_t(uint32_t ipv4) : _ipv4(ipv4) {}
        ipv4_addr_t(std::string_view ipv4)
            : _ipv4{boost::asio::ip::make_address_v4(ipv4).to_uint()} {}

        uint32_t get_raw_ipv4() const { return _ipv4; }
        bool     operator==(const ipv4_addr_t& other) const { return _ipv4 == other._ipv4; }

        size_t operator()(const ipv4_addr_t& p) const { return std::hash<uint32_t>()(p._ipv4); }

        static constexpr size_t size() { return 4; }

        void consume(std::byte*& ptr) { _ipv4 = utils::consume<uint32_t>(ptr); }

        void produce(std::byte*& ptr) { utils::produce<uint32_t>(ptr, _ipv4); }

        friend std::ostream& operator<<(std::ostream& out, ipv4_addr_t ipv4) {
                out << utils::format("{}.{}.{}.{}", (ipv4._ipv4 >> 24) & 0xFF,
                                     (ipv4._ipv4 >> 16) & 0xFF, (ipv4._ipv4 >> 8) & 0xFF,
                                     (ipv4._ipv4 >> 0) & 0xFF);
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
                return hash<uint32_t>{}(ipv4_addr.get_raw_ipv4());
        }
};
};  // namespace std
