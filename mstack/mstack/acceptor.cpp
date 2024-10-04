#include "acceptor.hpp"

#include <cassert>

#include <memory>
#include <utility>

#include <boost/system/error_code.hpp>

#include <spdlog/spdlog.h>

#include "ipv4_port.hpp"
#include "socket.hpp"
#include "tcb.hpp"
#include "tcb_manager.hpp"
#include "tcp.hpp"

namespace mstack {

acceptor::acceptor(netns& net, endpoint const& ep) : net_(net), ep_(ep) {
        net_.tcb_m().rule_insert_back(
                [=](ipv4_port_t const& remote_info, ipv4_port_t const& local_info) {
                        return local_info == ep.ep();
                },
                tcp::PROTO,
                [this](boost::system::error_code const& ec, ipv4_port_t const& remote_info,
                       ipv4_port_t const& local_info, std::weak_ptr<tcb_t> tcb) {
                        if (ec) {
                                spdlog::critical("failed to accept a new connection, reason: {}",
                                                 ec.what());
                                return;
                        }
                        if (!cbs_.empty()) {
                                auto cb{cbs_.front()};
                                cbs_.pop();
                                cb(remote_info, local_info, std::move(tcb));
                        }
                });
}

acceptor::acceptor(endpoint const& ep) : acceptor(netns::_default_(), ep) {}

acceptor::~acceptor() = default;

netns& acceptor::net() { return net_; }
netns& acceptor::net() const { return net_; }

void acceptor::async_accept(socket& sk, std::function<void(boost::system::error_code const&)> cb) {
        cbs_.push([&sk, cb = std::move(cb)](ipv4_port_t const&   remote_info,
                                            ipv4_port_t const&   local_info,
                                            std::weak_ptr<tcb_t> tcb) {
                sk.local_info = {tcp::PROTO, local_info};
                sk.state      = kSocketConnected;
                sk.tcb        = std::move(tcb);
                cb({});
        });
}

}  // namespace mstack
