#include <cassert>

#include <memory>
#include <stdexcept>
#include <utility>

#include <boost/system/error_code.hpp>

#include "netns.hpp"
#include "socket.hpp"

namespace mstack {

socket::socket(netns& net) : net_(net) {}

socket::socket() : socket(netns::_default_()) {}

void socket::async_connect(endpoint const&                                       remote_ep,
                           ipv4_addr_t const&                                    local_addr,
                           std::function<void(boost::system::error_code const&)> cb) {
        net_.tcb_m().async_connect(
                remote_ep, local_addr,
                [this, cb = std::move(cb)](boost::system::error_code const& ec,
                                           endpoint const& remote_ep [[maybe_unused]],
                                           endpoint const& local_ep, std::weak_ptr<tcb_t> tcb) {
                        if (ec) {
                                spdlog::critical("failed to accept a new connection, reason: {}",
                                                 ec.what());
                                return;
                        }

                        this->local_ep = local_ep;
                        this->tcb      = std::move(tcb);

                        cb({});
                });
}

void socket::async_read_some(std::span<std::byte>                                          buf,
                             std::function<void(boost::system::error_code const&, size_t)> cb) {
        if (auto sp = this->tcb.lock()) {
                sp->async_read_some(buf, std::move(cb));
        } else {
                throw std::runtime_error("endpoint is not connected");
        }
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
                        cb({}, 0);
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
        if (auto sp{this->tcb.lock()}) {
                sp->async_write(buf, std::move(cb));
        } else {
                throw std::runtime_error("endpoint is not connected");
        }
}

endpoint socket::remote_endpoint() const {
        if (auto sp{tcb.lock()}) return sp->remote_endpoint();
        throw std::runtime_error("endpoint is not connected");
}

}  // namespace mstack
