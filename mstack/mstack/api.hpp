#pragma once

#include <boost/asio/io_context.hpp>

#include "arp.hpp"
#include "ethernet.hpp"
#include "icmp.hpp"
#include "ipv4.hpp"
#include "socket_manager.hpp"
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

        arp::instance().register_dev(*dev);

        return dev;
}

inline std::unique_ptr<tun> tun_dev_create(boost::asio::io_context& io_ctx) {
        auto dev{tun::create(io_ctx)};

        dev->register_upper_protocol(ipv4::instance());

        return dev;
}

inline int socket(boost::asio::io_context& io_ctx,
                  int                      proto,
                  ipv4_addr_t              ipv4_addr,
                  port_addr_t              port_addr) {
        auto& socket_manager{socket_manager::instance()};
        return socket_manager.register_socket(io_ctx, proto, ipv4_addr, port_addr);
}

inline int listen(int fd) {
        auto& socket_manager{socket_manager::instance()};
        return socket_manager.listen(fd);
}

inline std::shared_ptr<socket_t> accept(boost::asio::io_context& io_ctx, int fd) {
        auto& socket_manager{socket_manager::instance()};
        return socket_manager.accept(io_ctx, fd);
}

}  // namespace mstack
