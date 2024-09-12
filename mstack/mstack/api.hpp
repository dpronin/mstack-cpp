#pragma once

#include <boost/asio/io_context.hpp>

#include "arp.hpp"
#include "ethernet.hpp"
#include "icmp.hpp"
#include "ipv4.hpp"
#include "tap.hpp"
#include "tcb_manager.hpp"
#include "tcp.hpp"
#include "tun.hpp"

namespace mstack {

inline auto& tcp_stack_create() {
        auto& stack{tcp::instance()};

        auto& tcb_manager{tcb_manager::instance()};
        stack.register_upper_protocol(tcb_manager);

        return stack;
}

inline auto& icmp_stack_create() {
        auto& stack{icmp::instance()};
        return stack;
}

inline auto& ipv4_stack_create() {
        auto& stack{ipv4::instance()};

        stack.register_upper_protocol(icmp::instance());
        stack.register_upper_protocol(tcp::instance());

        return stack;
}

inline auto& arpv4_stack_create() {
        auto& stack{arp::instance()};
        return stack;
}

inline auto& ethernetv2_stack_create() {
        auto& stack{ethernetv2::instance()};

        stack.register_upper_protocol(ipv4::instance());
        stack.register_upper_protocol(arp::instance());

        return stack;
}

inline void init_stack() {
        tcp_stack_create();
        icmp_stack_create();
        ipv4_stack_create();
        arpv4_stack_create();
        ethernetv2_stack_create();
}

template <size_t MTU>
inline std::unique_ptr<tap<MTU>> tap_dev_create(boost::asio::io_context& io_ctx) {
        auto dev{tap<MTU>::create(io_ctx)};

        dev->register_upper_protocol(ethernetv2::instance());

        return dev;
}

inline std::unique_ptr<tun> tun_dev_create(boost::asio::io_context& io_ctx) {
        auto dev{tun::create(io_ctx)};

        dev->register_upper_protocol(ipv4::instance());

        return dev;
}

}  // namespace mstack
