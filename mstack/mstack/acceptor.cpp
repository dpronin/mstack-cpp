#include "acceptor.hpp"

#include <cassert>

#include <memory>
#include <utility>

#include "socket.hpp"
#include "tcb.hpp"
#include "tcb_manager.hpp"

namespace mstack {

acceptor::acceptor(netns& net, int proto, endpoint const& ep) : sk_{std::make_unique<socket>(net)} {
        sk_->proto      = proto;
        sk_->local_info = {
                .ipv4_addr = ep.address(),
                .port_addr = ep.port(),
        };
        bind();
        listen();
}

acceptor::acceptor(int proto, endpoint const& ep) : acceptor(netns::_default_(), proto, ep) {}

acceptor::~acceptor() = default;

void acceptor::async_accept(socket& sk, std::function<void(boost::system::error_code const&)> cb) {
        auto f = [&sk, cb = std::move(cb)](std::shared_ptr<tcb_t> tcb) {
                assert(tcb);
                sk.proto       = tcb->_listener->proto;
                sk.local_info  = tcb->local_info;
                sk.remote_info = tcb->remote_info;
                sk.state       = SOCKET_CONNECTED;
                sk.tcb         = std::move(tcb);
                cb({});
        };

        if (auto listener{sk_->net.tcb_m().listener_get(sk_->local_info)};
            !listener->acceptors.empty()) {
                sk_->net.io_context_execution().post(
                        [f = std::move(f), tcb = listener->acceptors.pop_front().value()] mutable {
                                f(std::move(tcb));
                        });
        } else {
                listener->on_acceptor_has_tcb.push(f);
        }
}

void acceptor::bind() { sk_->net.tcb_m().bind(sk_->local_info); }

void acceptor::listen() {
        auto listener{std::make_unique<listener_t>()};

        listener->proto      = sk_->proto;
        listener->local_info = sk_->local_info;
        listener->state      = SOCKET_CONNECTING;

        sk_->net.tcb_m().listen(sk_->local_info, std::move(listener));
}

}  // namespace mstack
