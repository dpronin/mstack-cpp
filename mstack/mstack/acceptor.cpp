#include "acceptor.hpp"

#include <cassert>

#include <memory>
#include <utility>

#include "socket.hpp"
#include "tcb.hpp"
#include "tcb_manager.hpp"

namespace mstack {

acceptor::acceptor(netns& net, endpoint const& ep) : sk_{std::make_unique<socket>(net)} {
        sk_->local_info = ep;
        bind();
        listen();
}

acceptor::acceptor(endpoint const& ep) : acceptor(netns::_default_(), ep) {}

acceptor::~acceptor() = default;

netns& acceptor::net() { return sk_->net; }
netns& acceptor::net() const { return sk_->net; }

void acceptor::async_accept(socket& sk, std::function<void(boost::system::error_code const&)> cb) {
        auto f = [this, &sk, cb = std::move(cb)](std::weak_ptr<tcb_t> tcb) {
                sk.local_info = sk_->local_info;
                sk.state      = kSocketConnected;
                sk.tcb        = std::move(tcb);
                cb({});
        };

        if (auto listener{sk_->net.tcb_m().listener_get(sk_->local_info.ep())};
            !listener->acceptors.empty()) {
                sk_->net.io_context_execution().post(
                        [f = std::move(f), tcb = std::move(listener->acceptors.front())] mutable {
                                f(std::move(tcb));
                        });
                listener->acceptors.pop();
        } else {
                listener->on_acceptor_has_tcb.push(f);
        }
}

void acceptor::bind() { sk_->net.tcb_m().bind(sk_->local_info.ep()); }

void acceptor::listen() {
        auto listener{std::make_unique<listener_t>()};

        listener->proto      = sk_->local_endpoint().proto();
        listener->local_info = sk_->local_endpoint().ep();
        listener->state      = kSocketConnecting;

        sk_->net.tcb_m().listen(sk_->local_endpoint().ep(), std::move(listener));
}

}  // namespace mstack
