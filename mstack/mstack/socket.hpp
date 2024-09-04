#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>

#include <boost/asio/io_context.hpp>

#include "circle_buffer.hpp"
#include "defination.hpp"
#include "packets.hpp"

namespace mstack {

struct tcb_t;

struct socket_t {
        explicit socket_t(boost::asio::io_context& io_ctx) : io_ctx(io_ctx) {}
        ~socket_t() = default;

        socket_t(socket_t const&)            = delete;
        socket_t& operator=(socket_t const&) = delete;

        socket_t(socket_t&&)            = delete;
        socket_t& operator=(socket_t&&) = delete;

        boost::asio::io_context&   io_ctx;
        int                        fd;
        int                        state = SOCKET_UNCONNECTED;
        int                        proto;
        std::optional<ipv4_port_t> local_info;
        std::optional<ipv4_port_t> remote_info;
        std::shared_ptr<tcb_t>     tcb;

        ssize_t readsome(std::span<std::byte> buf);
        ssize_t write(std::span<std::byte const> buf);
};

struct listener_t {
        listener_t() : acceptors(std::make_shared<circle_buffer<std::shared_ptr<tcb_t>>>()) {}
        int                                                    fd;
        int                                                    state = SOCKET_UNCONNECTED;
        int                                                    proto;
        mutable std::mutex                                     lock;
        std::condition_variable                                cv;
        std::shared_ptr<circle_buffer<std::shared_ptr<tcb_t>>> acceptors;
        std::optional<ipv4_port_t>                             local_info;
};

}  // namespace mstack
