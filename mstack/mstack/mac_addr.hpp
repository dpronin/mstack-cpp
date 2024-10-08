#pragma once

#include <cstddef>

#include <algorithm>
#include <array>
#include <concepts>
#include <format>
#include <random>
#include <string>

#include <boost/container_hash/hash.hpp>

#include <fmt/format.h>

#include "utils.hpp"

namespace mstack {

struct mac_addr_t {
private:
        using mac = std::array<std::byte, 6>;
        mac mac_{};

public:
        static mac_addr_t generate() {
                static auto gen{std::mt19937{std::random_device{}()}};
                static auto dist{std::uniform_int_distribution<uint8_t>{0x00, 0xff}};

                std::array<std::byte, 6> hw_addr;
                std::ranges::generate(hw_addr, [] { return static_cast<std::byte>(dist(gen)); });
                hw_addr[0] = ((hw_addr[0] >> 1) | static_cast<std::byte>(0x1)) << 1;

                return mac_addr_t{hw_addr};
        }

        mac_addr_t()  = default;
        ~mac_addr_t() = default;

        mac_addr_t(const mac_addr_t& other)          = default;
        mac_addr_t& operator=(const mac_addr_t& rhs) = default;

        mac_addr_t(mac_addr_t&& other)          = default;
        mac_addr_t& operator=(mac_addr_t&& rhs) = default;

        template <typename T>
                requires(std::same_as<T, std::byte> || (std::integral<T> && 1 == sizeof(T)))
        mac_addr_t(std::array<T, 6> const& mac_from) {
                if constexpr (std::same_as<std::byte, T>)
                        std::ranges::copy(mac_from, mac_.begin());
                else
                        std::ranges::transform(mac_from, mac_.begin(),
                                               [](auto x) { return static_cast<std::byte>(x); });
        }

        template <typename T>
                requires(std::same_as<T, std::byte> || (std::integral<T> && 1 == sizeof(T)))
        mac_addr_t(T const (&mac_from)[6]) {
                if constexpr (std::same_as<std::byte, T>)
                        std::ranges::copy(mac_from, mac_.begin());
                else
                        std::ranges::transform(mac_from, mac_.begin(),
                                               [](auto x) { return static_cast<std::byte>(x); });
        }

        auto operator<=>(mac_addr_t const& other) const = default;

        bool is_broadcast() const {
                return std::ranges::all_of(
                        mac_, [](std::byte byte) { return static_cast<std::byte>(0xff) == byte; });
        }

        bool is_zero() const {
                return std::ranges::all_of(
                        mac_, [](std::byte byte) { return static_cast<std::byte>(0x0) == byte; });
        }

        void consume_from_net(std::byte*& ptr) {
                for (auto& d : mac_)
                        d = utils::consume_from_net<std::byte>(ptr);
        }

        void produce_to_net(std::byte*& ptr) const {
                for (auto d : mac_)
                        utils::produce_to_net<std::byte>(ptr, d);
        }

        mac const& raw() const { return mac_; }

        static constexpr size_t size() { return 6; }

        friend std::ostream& operator<<(std::ostream& out, const mac_addr_t& m) {
                using u = uint32_t;
                out << std::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", u(m.mac_[0]),
                                   u(m.mac_[1]), u(m.mac_[2]), u(m.mac_[3]), u(m.mac_[4]),
                                   u(m.mac_[5]));
                return out;
        }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::mac_addr_t> : fmt::formatter<std::string> {
        auto format(mstack::mac_addr_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};

namespace std {
template <>
struct hash<mstack::mac_addr_t> {
        size_t operator()(mstack::mac_addr_t const& mac_addr) const {
                auto const& mac{mac_addr.raw()};
                return boost::hash_range(mac.begin(), mac.end());
        }
};
}  // namespace std

namespace mstack {
inline size_t hash_value(mac_addr_t const& v) { return std::hash<mac_addr_t>{}(v); }
}  // namespace mstack
