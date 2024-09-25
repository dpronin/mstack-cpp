#include "acceptor.hpp"

#include <cassert>

#include <memory>
#include <utility>

#include <boost/system/error_code.hpp>

#include <spdlog/spdlog.h>

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
        sk_->net.tcb_m().async_accept(
                sk_->local_info,
                [this, &sk, cb = std::move(cb)](
                        boost::system::error_code const& ec, ipv4_port_t const& remote_info,
                        ipv4_port_t const& local_info, std::weak_ptr<tcb_t> tcb) {
                        if (ec) {
                                spdlog::critical("failed to accept a new connection, reason: {}",
                                                 ec.what());
                                return;
                        }
                        sk.local_info = sk_->local_info;
                        sk.state      = kSocketConnected;
                        sk.tcb        = std::move(tcb);
                        cb({});
                });
}

void acceptor::bind() { sk_->net.tcb_m().bind(sk_->local_info.ep()); }

void acceptor::listen() {}

}  // namespace mstack
