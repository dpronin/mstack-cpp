#pragma once

#include <cstddef>

#include <functional>
#include <memory>
#include <span>

#include <boost/system/error_code.hpp>

#include "defination.hpp"
#include "endpoint.hpp"

namespace mstack {

class tcb_t;
class netns;

class socket {
public:
        explicit socket(netns& net);
        socket();

        ~socket() = default;

        socket(socket const&)            = delete;
        socket& operator=(socket const&) = delete;

        socket(socket&&)            = delete;
        socket& operator=(socket&&) = delete;

        void async_connect(endpoint const&                                       remote_ep,
                           ipv4_addr_t const&                                    local_addr,
                           std::function<void(boost::system::error_code const&)> cb);

        void async_read_some(std::span<std::byte>                                          buf,
                             std::function<void(boost::system::error_code const&, size_t)> cb);

        void async_read(std::span<std::byte>                                          buf,
                        std::function<void(boost::system::error_code const&, size_t)> cb);

        void async_write_some(std::span<std::byte const>                                    buf,
                              std::function<void(boost::system::error_code const&, size_t)> cb);

        void async_write(std::span<std::byte const>                                    buf,
                         std::function<void(boost::system::error_code const&, size_t)> cb);

        endpoint const& local_endpoint() const { return local_ep; }

        endpoint remote_endpoint() const;

private:
        void async_read_complete(std::span<std::byte>                                          buf,
                                 std::function<void(boost::system::error_code const&, size_t)> cb);

        netns& net_;

public:
        int                  state{kSocketDisconnected};
        endpoint             local_ep;
        std::weak_ptr<tcb_t> tcb;
};

}  // namespace mstack
