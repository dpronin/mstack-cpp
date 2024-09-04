#include <cassert>

#include <memory>
#include <utility>

#include "socket.hpp"

#include "tcb.hpp"

namespace mstack {

void socket_t::async_read_some(std::span<std::byte>                                   buf,
                               std::function<void(boost::system::error_code, size_t)> cb) {
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

ssize_t socket_t::read_some(std::span<std::byte> buf) {
        assert(this->tcb);

        auto r_packet{
                std::move(this->tcb->receive_queue.pop_front().value()),
        };

        return r_packet.buffer->export_data(buf);
}

void socket_t::async_write(std::span<std::byte const>                             buf,
                           std::function<void(boost::system::error_code, size_t)> cb) {
        cb({}, write(buf));
}

ssize_t socket_t::write(std::span<std::byte const> buf) {
        assert(this->tcb);
        this->tcb->enqueue_send({.buffer = std::make_unique<base_packet>(buf)});
        return buf.size();
}

}  // namespace mstack
