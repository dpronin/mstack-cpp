#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <functional>
#include <queue>

#include "endpoint.hpp"
#include "netns.hpp"
#include "raw_socket.hpp"

namespace mstack {

struct raw_socket;

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

        void async_intercept(std::function<bool(tcp_packet const& pkt_in)> cb);

        netns& net();
        netns& net() const;

private:
        netns&                                                    net_;
        std::queue<std::function<bool(tcp_packet const& pkt_in)>> cbs_;
};

}  // namespace mstack
