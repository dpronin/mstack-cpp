#include "acceptor.hpp"

#include <cassert>

#include <memory>
#include <utility>

#include <boost/system/error_code.hpp>

#include <spdlog/spdlog.h>

#include "endpoint.hpp"
#include "socket.hpp"
#include "tcb.hpp"
#include "tcb_manager.hpp"
#include "tcp.hpp"

namespace mstack {

acceptor::acceptor(
        netns&                                                                          net,
        std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> const& matcher)
    : net_(net) {
        net_.tcb_m().rule_insert_back(
                matcher, tcp::PROTO,
                [this](boost::system::error_code const& ec, endpoint const& remote_ep,
                       endpoint const& local_ep, std::weak_ptr<tcb_t> tcb) {
                        if (ec) {
                                spdlog::critical("failed to accept a new connection, reason: {}",
                                                 ec.what());
                                return;
                        }
                        if (!cbs_.empty()) {
                                auto cb{cbs_.front()};
                                cbs_.pop();
                                cb(remote_ep, local_ep, std::move(tcb));
                        }
                });
}

acceptor::acceptor(netns& net, endpoint const& local_ep)
    : acceptor(net,
               [local_ep_exp = local_ep](endpoint const& remote_ep [[maybe_unused]],
                                         endpoint const& local_ep) {
                       return local_ep == local_ep;
               }) {}

acceptor::acceptor(endpoint const& local_ep) : acceptor(netns::_default_(), local_ep) {}

acceptor::~acceptor() = default;

netns& acceptor::net() { return net_; }
netns& acceptor::net() const { return net_; }

void acceptor::async_accept(socket& sk, std::function<void(boost::system::error_code const&)> cb) {
        cbs_.push([&sk, cb = std::move(cb)](endpoint const& remote_ep, endpoint const& local_ep,
                                            std::weak_ptr<tcb_t> tcb) {
                sk.local_ep = local_ep;
                sk.state    = kSocketConnected;
                sk.tcb      = std::move(tcb);
                cb({});
        });
}

}  // namespace mstack
