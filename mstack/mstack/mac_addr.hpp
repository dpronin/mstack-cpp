#pragma once

#include <cstddef>

#include <algorithm>
#include <array>
#include <concepts>
#include <format>
#include <string>

#include "utils.hpp"

namespace mstack {

struct mac_addr_t {
private:
        using mac = std::array<std::byte, 6>;
        mac mac_{};

public:
        mac_addr_t() = default;

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
