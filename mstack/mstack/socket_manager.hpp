#include <cassert>

#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/system/detail/errc.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/errc.hpp>

#include "defination.hpp"
#include "socket.hpp"
#include "tcb_manager.hpp"

namespace mstack {

class socket_manager {
private:
        socket_manager()  = default;
        ~socket_manager() = default;
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

        int register_socket(boost::asio::io_context& io_ctx,
                            int                      proto,
                            ipv4_addr_t              ipv4_addr,
                            port_addr_t              port_addr) {
                for (int i = 1; i < 65535; i++) {
                        if (!sockets.contains(i)) {
                                ipv4_port_t local_info{
                                        .ipv4_addr = ipv4_addr,
                                        .port_addr = port_addr,
                                };

                                auto socket{std::make_shared<socket_t>(io_ctx)};

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

        std::shared_ptr<socket_t> accept(boost::asio::io_context& io_ctx, int fd) {
                if (!listeners.contains(fd)) return {};

                auto listener{listeners[fd]};

                if (!listener->acceptors->empty()) {
                        for (int i = 1; i < 65535; i++) {
                                if (!sockets.contains(i)) {
                                        if (auto tcb{listener->acceptors->pop_front().value()}) {
                                                auto socket{std::make_shared<socket_t>(io_ctx)};

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
                }

                return {};
        }

        void async_accept(
                boost::asio::io_context&                                                  io_ctx,
                int                                                                       fd,
                std::function<void(boost::system::error_code const&, std::shared_ptr<socket_t>)> cb) {
                if (!listeners.contains(fd)) return;

                auto l{listeners[fd]};

                auto f = [this, &io_ctx, wl = std::weak_ptr{l}, cb = std::move(cb)] {
                        if (auto l = wl.lock()) {
                                assert(!l->acceptors->empty());
                                for (int i = 1; i < 65535; i++) {
                                        if (!sockets.contains(i)) {
                                                if (auto tcb{l->acceptors->pop_front().value()}) {
                                                        auto socket{
                                                                std::make_shared<socket_t>(io_ctx),
                                                        };

                                                        socket->local_info  = tcb->local_info;
                                                        socket->remote_info = tcb->remote_info;
                                                        socket->proto       = l->proto;
                                                        socket->state       = SOCKET_CONNECTED;
                                                        socket->tcb         = std::move(tcb);
                                                        socket->fd          = i;
                                                        sockets[i]          = socket;

                                                        cb({}, std::move(socket));
                                                        return;
                                                }
                                                break;
                                        }
                                }
                        }
                        cb(boost::system::errc::make_error_code(
                                   boost::system::errc::connection_reset),
                           {});
                };

                if (!l->on_acceptor_has_tcb.empty())
                        io_ctx.post(f);
                else
                        l->on_acceptor_has_tcb.push(f);
        }
};

}  // namespace mstack
