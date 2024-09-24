#pragma once

#include <cassert>
#include <cstdint>

#include <algorithm>
#include <bit>
#include <format>
#include <functional>
#include <span>
#include <string>

#include <net/if.h>

#include <netlink/addr.h>
#include <netlink/cache.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/nexthop.h>
#include <netlink/route/route.h>
#include <netlink/socket.h>

#include <spdlog/spdlog.h>

namespace mstack {

namespace utils {

namespace detail {

using sock_ptr = std::unique_ptr<nl_sock, std::function<void(nl_sock*)>>;

inline auto sock_alloc() noexcept {
        return std::unique_ptr<nl_sock, decltype(&::nl_socket_free)>{
                ::nl_socket_alloc(),
                ::nl_socket_free,
        };
}

inline sock_ptr sock_open() noexcept {
        auto sock{sock_alloc()};
        if (::nl_connect(sock.get(), NETLINK_ROUTE) < 0) return {};
        return {
                sock.release(),
                [d = sock.get_deleter()](nl_sock* psock) {
                        ::nl_close(psock);
                        d(psock);
                },
        };
}

}  // namespace detail

template <typename... A>
inline static int run_cmd(std::format_string<A...> fmt, A&&... a) {
        std::string cmd{std::format(fmt, std::forward<A>(a)...)};
        spdlog::info("[EXEC COMMAND]: {}", cmd);
        return system(cmd.c_str());
}

inline int set_interface_route(std::string_view dev, std::string_view cidr) {
        spdlog::info("[INSTALLING ROUTE]: {} via {}", cidr, dev);

        auto sock{detail::sock_open()};
        if (!sock) return -ENOMEM;

        auto route{
                std::unique_ptr<::rtnl_route, decltype(&::rtnl_route_put)>{
                        rtnl_route_alloc(),
                        rtnl_route_put,
                },
        };

        rtnl_route_set_iif(route.get(), AF_INET);  // IPV4
        rtnl_route_set_scope(route.get(), RT_SCOPE_LINK);
        rtnl_route_set_table(route.get(), RT_TABLE_LOCAL);
        rtnl_route_set_protocol(route.get(), RTPROT_STATIC);

        auto dst{
                std::unique_ptr<::nl_addr, decltype(&::nl_addr_put)>{
                        nullptr,
                        ::nl_addr_put,
                },
        };
        ::nl_addr* pdst{nullptr};
        if (auto const r{nl_addr_parse(std::string{cidr}.data(), AF_INET, &pdst)}; !(r < 0)) {
                dst.reset(std::exchange(pdst, nullptr));
        } else {
                return r;
        }

        if (auto const r{rtnl_route_set_dst(route.get(), dst.get())}; r < 0) {
                return r;
        }

        auto nh{
                std::unique_ptr<::rtnl_nexthop, decltype(&::rtnl_route_nh_free)>{
                        rtnl_route_nh_alloc(),
                        &::rtnl_route_nh_free,
                },
        };

        auto link{
                std::unique_ptr<::rtnl_link, decltype(&::rtnl_link_put)>{
                        nullptr,
                        ::rtnl_link_put,
                },
        };
        ::rtnl_link* p_link{nullptr};
        if (auto const r{rtnl_link_get_kernel(sock.get(), 0, std::string{dev}.data(), &p_link)};
            !(r < 0)) {
                link.reset(std::exchange(p_link, nullptr));
        } else {
                return r;
        }

        rtnl_route_nh_set_ifindex(nh.get(), rtnl_link_get_ifindex(link.get()));
        rtnl_route_add_nexthop(route.get(), nh.get());

        if (auto const r{rtnl_route_add(sock.get(), route.release(), 0)}; r < 0) {
                return r;
        }

        return 0;
}

inline int set_interface_address(std::string dev, std::string cidr) {
        return run_cmd("ip address add dev {} local {}", dev, cidr);
}

inline int set_interface_up(std::string_view dev) {
        spdlog::info("[SETTING DEVICE UP]: {}", dev);

        auto sock{detail::sock_open()};
        if (!sock) return -ENOMEM;

        auto link{
                std::unique_ptr<::rtnl_link, decltype(&::rtnl_link_put)>{
                        nullptr,
                        ::rtnl_link_put,
                },
        };
        ::rtnl_link* p_link{nullptr};
        if (auto const r{rtnl_link_get_kernel(sock.get(), 0, dev.data(), &p_link)}; !(r < 0)) {
                link.reset(std::exchange(p_link, nullptr));
        } else {
                return r;
        }

        auto new_link{decltype(link){rtnl_link_alloc(), ::rtnl_link_put}};
        rtnl_link_set_flags(new_link.get(), IFF_UP);

        if (auto const r{rtnl_link_change(sock.get(), link.get(), new_link.get(), 0)}; r < 0) {
                return r;
        }

        return 0;
}

template <std::integral T>
constexpr auto ntoh(T value) {
        auto x{std::bit_cast<std::array<std::byte, sizeof(T)>>(value)};
        std::ranges::reverse(x);
        return std::bit_cast<T>(x);
}

template <std::integral T>
constexpr void ntoh_inplace(T& value) {
        value = ntoh(value);
}

template <typename T>
constexpr T consume(std::byte*& ptr) {
        T ret = *(reinterpret_cast<T*>(ptr));
        ptr += sizeof(T);
        if constexpr (sizeof(ret) > 1)
                return ntoh(ret);
        else
                return ret;
}

template <typename T>
constexpr void produce(std::byte*& p, T t) {
        T* ptr_ = reinterpret_cast<T*>(p);
        if constexpr (sizeof(T) > 1)
                *ptr_ = ntoh(t);
        else
                *ptr_ = t;
        p += sizeof(T);
}

inline uint32_t sum_every_16bits(std::span<std::byte const> addr) {
        uint32_t sum = 0;

        assert(!(reinterpret_cast<uintptr_t>(addr.data()) & 0x1));

        auto const* ptr = reinterpret_cast<uint16_t const*>(addr.data());

        auto count{addr.size()};
        while (count > 1) {
                /*  This is the inner loop */
                sum += *ptr++;
                count -= 2;
        }

        /*  Add left-over byte, if any */
        if (count > 0) sum += *(uint8_t*)ptr;

        return sum;
}

inline uint16_t checksum(std::span<std::byte const> addr, uint32_t start_sum) {
        uint32_t sum = start_sum + sum_every_16bits(addr);

        /*  Fold 32-bit sum to 16 bits */
        while (sum >> 16)
                sum = (sum & 0xffff) + (sum >> 16);

        return ntoh(static_cast<uint16_t>(~sum));
}

}  // namespace utils

}  // namespace mstack
