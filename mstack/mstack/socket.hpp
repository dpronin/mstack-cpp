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

namespace mstack {

struct tcb_t;
class tcb_manager;
class netns;

class socket {
public:
        explicit socket(netns& net) : net(net) {}
        socket();

        ~socket() = default;

        socket(socket const&)            = delete;
        socket& operator=(socket const&) = delete;

        socket(socket&&)            = delete;
        socket& operator=(socket&&) = delete;

        void async_connect(endpoint const&                                       ep,
                           std::function<void(boost::system::error_code const&)> cb);

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

public:
        netns&                     net;
        int                        fd{-1};
        int                        state{SOCKET_UNCONNECTED};
        int                        proto{-1};
        ipv4_port_t                local_info;
        std::optional<ipv4_port_t> remote_info;
        std::shared_ptr<tcb_t>     tcb;
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
