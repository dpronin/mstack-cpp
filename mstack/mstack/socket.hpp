#pragma once

#include <cstddef>

#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <span>

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <sys/types.h>

#include "circle_buffer.hpp"
#include "defination.hpp"
#include "endpoint.hpp"
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
        ipv4_port_t                local_info;
        std::optional<ipv4_port_t> remote_info;
        std::shared_ptr<tcb_t>     tcb;

        void async_read_some(std::span<std::byte>                                          buf,
                             std::function<void(boost::system::error_code const&, size_t)> cb);

        void async_read(std::span<std::byte>                                          buf,
                        std::function<void(boost::system::error_code const&, size_t)> cb);

        void async_write_some(std::span<std::byte const>                                    buf,
                              std::function<void(boost::system::error_code const&, size_t)> cb);

        void async_write(std::span<std::byte const>                                    buf,
                         std::function<void(boost::system::error_code const&, size_t)> cb);

        endpoint local_endpoint() const { return {local_info}; }

        endpoint remote_endpoint() const { return {remote_info.value()}; }

private:
        void async_read_complete(std::span<std::byte>                                          buf,
                                 std::function<void(boost::system::error_code const&, size_t)> cb);
};

struct listener_t {
        listener_t() {}
        int                                   fd;
        int                                   state{SOCKET_UNCONNECTED};
        int                                   proto;
        circle_buffer<std::shared_ptr<tcb_t>> acceptors;
        std::queue<std::function<void()>>     on_acceptor_has_tcb;
        ipv4_port_t                           local_info;
};

}  // namespace mstack
