#include <cassert>

#include <memory>
#include <mutex>
#include <utility>

#include "socket.hpp"

#include "tcb.hpp"

namespace mstack {

ssize_t socket_t::read(std::span<std::byte> buf) {
        assert(this->tcb);

        std::unique_lock l2{this->tcb->receive_queue_lock};
        this->tcb->receive_queue_cv.wait(l2, [this] { return !this->tcb->receive_queue.empty(); });
        auto r_packet{
                std::move(this->tcb->receive_queue.pop_front().value()),
        };
        l2.unlock();

        return r_packet.buffer->export_data(buf);
}

ssize_t socket_t::write(std::span<std::byte const> buf) {
        assert(this->tcb);
        this->tcb->enqueue_send({.buffer = std::make_unique<base_packet>(buf)});
        return buf.size();
}

}  // namespace mstack
