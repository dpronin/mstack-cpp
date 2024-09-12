#include "socket_manager.hpp"

#include "socket.hpp"

namespace mstack {

std::unique_ptr<socket_t> socket_manager::socket_create(boost::asio::io_context& io_ctx) {
        return std::make_unique<socket_t>(io_ctx);
}

}  // namespace mstack
