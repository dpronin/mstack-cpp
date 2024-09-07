#include <cassert>

#include <memory>
#include <utility>

#include "socket.hpp"

#include "tcb.hpp"

namespace mstack {

void socket_t::async_read_some(std::span<std::byte>                                          buf,
                               std::function<void(boost::system::error_code const&, size_t)> cb) {
        auto f = [this, buf, cb = std::move(cb)] {
                assert(!this->tcb->receive_queue.empty());
                auto pkt{this->tcb->receive_queue.pop_front().value()};
                cb({}, pkt.buffer->export_data(buf));
        };

        if (!this->tcb->receive_queue.empty())
                io_ctx.post(f);
        else
                this->tcb->on_data_receive.push(f);
}

void socket_t::async_read(std::span<std::byte>                                          rbuf,
                          std::function<void(boost::system::error_code const&, size_t)> cb) {
        async_read_complete(rbuf, [nbytesall = rbuf.size(), cb = std::move(cb)](
                                          auto const& ec, size_t nbytes_left) {
                cb(ec, nbytesall - nbytes_left);
        });
}

void socket_t::async_read_complete(
        std::span<std::byte>                                          rbuf,
        std::function<void(boost::system::error_code const&, size_t)> cb) {
        async_read_some(rbuf, [this, rbuf, cb = std::move(cb)](boost::system::error_code const& ec,
                                                               size_t nbytes) mutable {
                if (ec) {
                        cb(ec, rbuf.size() - nbytes);
                        return;
                }

                if (!(nbytes < rbuf.size())) {
                        cb(boost::system::error_code{std::error_code{}}, 0);
                } else {
                        async_read_complete(rbuf.subspan(nbytes), std::move(cb));
                }
        });
}

void socket_t::async_write_some(std::span<std::byte const>                                    buf,
                                std::function<void(boost::system::error_code const&, size_t)> cb) {
        async_write(buf, std::move(cb));
}

void socket_t::async_write(std::span<std::byte const>                                    buf,
                           std::function<void(boost::system::error_code const&, size_t)> cb) {
        assert(this->tcb);
        this->tcb->enqueue_send(buf);
        io_ctx.post([sz = buf.size(), cb = std::move(cb)] { cb({}, sz); });
}

}  // namespace mstack
