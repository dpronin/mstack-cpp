#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <functional>
#include <queue>

#include "endpoint.hpp"
#include "netns.hpp"
#include "raw_socket.hpp"

namespace mstack {

class interceptor {
public:
        explicit interceptor(netns&                                               net,
                             std::function<bool(endpoint const& remote_ep,
                                                endpoint const& local_ep)> const& matcher);
        explicit interceptor(netns& net, endpoint const& local_ep);
        explicit interceptor(endpoint const& local_ep);
        ~interceptor();

        interceptor(interceptor const&)            = delete;
        interceptor& operator=(interceptor const&) = delete;

        interceptor(interceptor&&)            = delete;
        interceptor& operator=(interceptor&&) = delete;

        void async_intercept(std::function<void(std::unique_ptr<raw_socket> sk)> cb);

        netns& net();
        netns& net() const;

private:
        netns& net_;
        std::queue<std::function<void(endpoint const&                     remote_ep,
                                      endpoint const&                     local_ep,
                                      std::shared_ptr<raw_socket::pqueue> rcv_pq)>>
                cbs_;
};

}  // namespace mstack
