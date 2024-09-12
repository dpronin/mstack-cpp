#include <cassert>
#include <cstdint>

#include <unordered_set>

#include <boost/asio/io_context.hpp>

#include "socket.hpp"

namespace mstack {

inline std::unordered_set<uint16_t> __sockets__;

class socket_manager {
private:
        socket_manager()  = default;
        ~socket_manager() = default;

public:
        socket_manager(const socket_manager&)            = delete;
        socket_manager(socket_manager&&)                 = delete;
        socket_manager& operator=(const socket_manager&) = delete;
        socket_manager& operator=(socket_manager&&)      = delete;

        static socket_manager& instance() {
                static socket_manager instance;
                return instance;
        }

        std::unique_ptr<socket_t> socket_create(boost::asio::io_context& io_ctx);
};

}  // namespace mstack
