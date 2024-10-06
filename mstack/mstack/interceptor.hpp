#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <functional>
#include <queue>

#include "ipv4_port.hpp"
#include "netns.hpp"
#include "raw_socket.hpp"

namespace mstack {

struct endpoint;
struct raw_socket;

class interceptor {
public:
        explicit interceptor(netns&                                                    net,
                             std::function<bool(ipv4_port_t const& remote_info,
                                                ipv4_port_t const& local_info)> const& matcher);
        explicit interceptor(netns& net, endpoint const& ep);
        explicit interceptor(endpoint const& ep);
        ~interceptor();

        interceptor(interceptor const&)            = delete;
        interceptor& operator=(interceptor const&) = delete;

        interceptor(interceptor&&)            = delete;
        interceptor& operator=(interceptor&&) = delete;

        void async_intercept(raw_socket& sk, std::function<bool(tcp_packet const& pkt_in)> cb);

        netns& net();
        netns& net() const;

private:
        netns&                                                    net_;
        std::queue<std::function<bool(tcp_packet const& pkt_in)>> cbs_;
};

}  // namespace mstack
