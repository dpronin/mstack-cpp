#include <cassert>

#include <memory>
#include <stdexcept>
#include <utility>

#include "netns.hpp"
#include "socket.hpp"

namespace mstack {

socket::socket() : net(netns::_default_()) {}

void socket::async_connect(endpoint const&                                       ep,
                           std::function<void(boost::system::error_code const&)> cb) {
        throw std::runtime_error("function is not implemented");
}

void socket::async_read_some(std::span<std::byte>                                          buf,
                             std::function<void(boost::system::error_code const&, size_t)> cb) {
        this->tcb->async_read_some(buf, std::move(cb));
}

void socket::async_read(std::span<std::byte>                                          buf,
                        std::function<void(boost::system::error_code const&, size_t)> cb) {
        async_read_complete(buf, [nbytesall = buf.size(), cb = std::move(cb)](auto const& ec,
                                                                              size_t nbytes_left) {
                cb(ec, nbytesall - nbytes_left);
        });
}

void socket::async_read_complete(std::span<std::byte>                                          buf,
                                 std::function<void(boost::system::error_code const&, size_t)> cb) {
        async_read_some(buf, [this, buf, cb = std::move(cb)](boost::system::error_code const& ec,
                                                             size_t nbytes) mutable {
                if (ec) {
                        cb(ec, buf.size() - nbytes);
                        return;
                }

                if (!(nbytes < buf.size())) {
                        cb(boost::system::error_code{std::error_code{}}, 0);
                } else {
                        async_read_complete(buf.subspan(nbytes), std::move(cb));
                }
        });
}

void socket::async_write_some(std::span<std::byte const>                                    buf,
                              std::function<void(boost::system::error_code const&, size_t)> cb) {
        async_write(buf, std::move(cb));
}

void socket::async_write(std::span<std::byte const>                                    buf,
                         std::function<void(boost::system::error_code const&, size_t)> cb) {
        assert(this->tcb);
        tcb->async_write(buf, std::move(cb));
}

}  // namespace mstack
