#include <cassert>
#include <condition_variable>
#include <mutex>
#include <unordered_map>

#include "defination.hpp"
#include "socket.hpp"
#include "tcb_manager.hpp"

namespace mstack {

class socket_manager {
private:
        socket_manager()  = default;
        ~socket_manager() = default;
        mutable std::mutex                                        lock;
        std::unordered_map<uint16_t, std::shared_ptr<socket_t>>   sockets;
        std::unordered_map<uint16_t, std::shared_ptr<listener_t>> listeners;

public:
        socket_manager(const socket_manager&)            = delete;
        socket_manager(socket_manager&&)                 = delete;
        socket_manager& operator=(const socket_manager&) = delete;
        socket_manager& operator=(socket_manager&&)      = delete;

        static socket_manager& instance() {
                static socket_manager instance;
                return instance;
        }

        int register_socket(int proto, ipv4_addr_t ipv4_addr, port_addr_t port_addr) {
                for (int i = 1; i < 65535; i++) {
                        if (!sockets.contains(i)) {
                                ipv4_port_t local_info{
                                        .ipv4_addr = ipv4_addr,
                                        .port_addr = port_addr,
                                };

                                std::shared_ptr<socket_t> socket = std::make_shared<socket_t>();

                                socket->proto      = proto;
                                socket->local_info = local_info;
                                sockets[i]         = socket;

                                return i;
                        }
                }
                return -1;
        }

        int listen(int fd) {
                if (!sockets.contains(fd)) return -1;

                auto listener{std::make_shared<listener_t>()};
                listener->local_info = sockets[fd]->local_info;
                listener->proto      = sockets[fd]->proto;
                listener->state      = SOCKET_CONNECTING;
                listener->fd         = fd;
                listeners[fd]        = listener;

                auto& tcb_manager = tcb_manager::instance();
                tcb_manager.listen_port(listener->local_info.value(), listener);

                return 0;
        };

        std::shared_ptr<socket_t> accept(int fd) {
                if (!listeners.contains(fd)) return {};

                auto listener{listeners[fd]};

                std::unique_lock l{listener->lock};
                listener->cv.wait(l, [listener] { return !listener->acceptors->empty(); });

                for (int i = 1; i < 65535; i++) {
                        if (!sockets.contains(i)) {
                                if (auto tcb{listener->acceptors->pop_front().value()}) {
                                        auto socket{std::make_shared<socket_t>()};

                                        socket->local_info  = tcb->local_info;
                                        socket->remote_info = tcb->remote_info;
                                        socket->proto       = listener->proto;
                                        socket->state       = SOCKET_CONNECTED;
                                        socket->tcb         = std::move(tcb);
                                        socket->fd          = i;
                                        sockets[i]          = socket;

                                        return socket;
                                }
                                break;
                        }
                }

                l.unlock();

                return {};
        }

        ssize_t read(socket_t& sk, std::span<std::byte> buf) {
                std::unique_lock l1{lock};
                if (!sockets.contains(sk.fd)) return -ENOENT;
                l1.unlock();

                assert(sk.tcb);

                std::unique_lock l2{sk.tcb->receive_queue_lock};
                sk.tcb->receive_queue_cv.wait(l2, [&sk] { return !sk.tcb->receive_queue.empty(); });
                auto r_packet{
                        std::move(sk.tcb->receive_queue.pop_front().value()),
                };
                l2.unlock();

                return r_packet.buffer->export_data(buf);
        }

        ssize_t write(socket_t& sk, std::span<std::byte const> buf) {
                std::unique_lock l{lock};
                if (!sockets.contains(sk.fd)) return -1;
                l.unlock();

                assert(sk.tcb);
                sk.tcb->enqueue_send({.buffer = std::make_unique<base_packet>(buf)});

                return buf.size();
        }
};

}  // namespace mstack
