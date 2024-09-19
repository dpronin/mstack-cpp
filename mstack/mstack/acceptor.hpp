#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <memory>

#include "netns.hpp"

namespace mstack {

struct socket;
struct endpoint;
class tcb_manager;

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

private:
        void bind();
        void listen();

        std::unique_ptr<socket> sk_;
};

}  // namespace mstack
