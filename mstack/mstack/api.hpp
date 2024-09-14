#pragma once

#include <cstddef>

#include <boost/asio/io_context.hpp>

#include "arp.hpp"
#include "ethernet.hpp"
#include "icmp.hpp"
#include "ipv4.hpp"
#include "tcb_manager.hpp"
#include "tcp.hpp"

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

}  // namespace mstack
