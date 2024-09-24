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
        netns&                  net;
        int                     state{SOCKET_UNCONNECTED};
        endpoint                local_info;
        std::optional<endpoint> remote_info;
        std::shared_ptr<tcb_t>  tcb;
};

struct listener_t {
        int                                                     proto;
        ipv4_port_t                                             local_info;
        int                                                     state{SOCKET_UNCONNECTED};
        std::queue<std::shared_ptr<tcb_t>>                      acceptors;
        std::queue<std::function<void(std::shared_ptr<tcb_t>)>> on_acceptor_has_tcb;
};

}  // namespace mstack
