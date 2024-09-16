#include "acceptor.hpp"

#include <stdexcept>

#include "socket_manager.hpp"

#include "socket.hpp"
#include "tcb_manager.hpp"

namespace mstack {

acceptor::acceptor(netns& net, int proto, endpoint const& ep) {
        for (uint16_t fd = 1; fd > 0; ++fd) {
                if (!__sockets__.contains(fd)) {
                        sk_ = std::make_unique<socket_t>(net);

                        sk_->proto      = proto;
                        sk_->local_info = {
                                .ipv4_addr = ep.address(),
                                .port_addr = ep.port(),
                        };

                        __sockets__.insert(fd);

                        listen();

                        return;
                }
        }

        throw std::overflow_error{"no available ports for a new acceptor"};
}

acceptor::acceptor(int proto, endpoint const& ep) : acceptor(netns::_default_(), proto, ep) {}

acceptor::~acceptor() = default;

void acceptor::async_accept(socket_t&                                             sk,
                            std::function<void(boost::system::error_code const&)> cb) {
        auto f = [this, &sk, cb = std::move(cb)] {
                if (auto& l{sk.net.tcb_m().listener_get(sk_->local_info)}; !l.acceptors.empty()) {
                        for (int i = 1; i < 65535; i++) {
                                if (!__sockets__.contains(i)) {
                                        if (auto tcb{l.acceptors.pop_front().value()}) {
                                                sk.local_info  = tcb->local_info;
                                                sk.remote_info = tcb->remote_info;
                                                sk.proto       = l.proto;
                                                sk.state       = SOCKET_CONNECTED;
                                                sk.tcb         = std::move(tcb);
                                                sk.fd          = i;
                                                cb({});
                                                return;
                                        }
                                        break;
                                }
                        }
                }
                cb(boost::system::errc::make_error_code(boost::system::errc::connection_reset));
        };

        if (auto& l{sk_->net.tcb_m().listener_get(sk_->local_info)};
            !l.on_acceptor_has_tcb.empty()) {
                sk_->net.io_context_execution().post(f);
        } else {
                l.on_acceptor_has_tcb.push(f);
        }
}

void acceptor::listen() {
        auto listener{std::make_unique<listener_t>()};

        listener->local_info = sk_->local_info;
        listener->proto      = sk_->proto;
        listener->state      = SOCKET_CONNECTING;
        listener->fd         = sk_->fd;

        sk_->net.tcb_m().listen_port(sk_->local_info, std::move(listener));
}

}  // namespace mstack
