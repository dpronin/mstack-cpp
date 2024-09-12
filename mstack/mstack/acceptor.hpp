#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <memory>

#include "socket.hpp"

namespace mstack {

class acceptor {
public:
        explicit acceptor(boost::asio::io_context& io_ctx, int proto, endpoint const& ep);
        ~acceptor() = default;

        acceptor(acceptor const&)            = delete;
        acceptor& operator=(acceptor const&) = delete;

        acceptor(acceptor&&)            = delete;
        acceptor& operator=(acceptor&&) = delete;

        void async_accept(mstack::socket_t&                                     sk,
                          std::function<void(boost::system::error_code const&)> cb);

private:
        void listen();

        std::unique_ptr<socket_t> sk_;
};

}  // namespace mstack
