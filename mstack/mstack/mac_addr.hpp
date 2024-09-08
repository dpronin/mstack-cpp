#pragma once

#include <cstddef>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include "utils.hpp"

namespace mstack {

struct mac_addr_t {
private:
        std::array<std::byte, 6> mac{};

public:
        mac_addr_t() = default;

        mac_addr_t(const mac_addr_t& other)          = default;
        mac_addr_t& operator=(const mac_addr_t& rhs) = default;

        mac_addr_t(mac_addr_t&& other)          = default;
        mac_addr_t& operator=(mac_addr_t&& rhs) = default;

        explicit mac_addr_t(std::array<std::byte, 6> const& mac_from) {
                std::ranges::copy(mac_from, mac.begin());
        }

        explicit mac_addr_t(std::byte const (&mac_from)[6]) {
                std::ranges::copy(mac_from, mac.begin());
        }

        explicit mac_addr_t(std::string_view mac_from) {
                for (int i = 0; i < 17; i += 3) {
                        mac[i / 3] = static_cast<std::byte>(
                                std::stoi(std::string{mac_from.substr(i, 2)}, 0, 16));
                }
        };

        void consume(std::byte*& ptr) {
                for (auto& d : mac)
                        d = utils::consume<std::byte>(ptr);
        }

        void produce(std::byte*& ptr) const {
                for (auto d : mac)
                        utils::produce<std::byte>(ptr, d);
        }

        static constexpr size_t size() { return 6; }

        friend std::ostream& operator<<(std::ostream& out, const mac_addr_t& m) {
                using u = uint32_t;
                out << utils::format("{:2x}:{:2x}:{:2x}:{:2x}:{:2x}:{:2x}", u(m.mac[0]),
                                     u(m.mac[1]), u(m.mac[2]), u(m.mac[3]), u(m.mac[4]),
                                     u(m.mac[5]));
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
