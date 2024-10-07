#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <functional>
#include <queue>

#include "endpoint.hpp"
#include "netns.hpp"

namespace mstack {

struct socket;

class acceptor {
public:
        explicit acceptor(netns&                                               net,
                          std::function<bool(endpoint const& remote_ep,
                                             endpoint const& local_ep)> const& matcher);
        explicit acceptor(netns& net, endpoint const& local_ep);
        explicit acceptor(endpoint const& local_ep);
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
        std::queue<std::function<void(endpoint const&, endpoint const&, std::weak_ptr<tcb_t>)>>
                cbs_;
};

}  // namespace mstack
