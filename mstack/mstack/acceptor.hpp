#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <functional>
#include <queue>

#include "ipv4_port.hpp"
#include "netns.hpp"

namespace mstack {

struct socket;
struct endpoint;

class acceptor {
public:
        explicit acceptor(netns& net, endpoint const& ep);
        explicit acceptor(endpoint const& ep);
        ~acceptor();

        acceptor(acceptor const&)            = delete;
        acceptor& operator=(acceptor const&) = delete;

        acceptor(acceptor&&)            = delete;
        acceptor& operator=(acceptor&&) = delete;

        void async_accept(socket& sk, std::function<void(boost::system::error_code const&)> cb);

        netns& net();
        netns& net() const;

private:
        netns& net_;
        std::queue<
                std::function<void(ipv4_port_t const&, ipv4_port_t const&, std::weak_ptr<tcb_t>)>>
                cbs_;
};

}  // namespace mstack
