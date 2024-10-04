#include "interceptor.hpp"

#include <cassert>

#include <utility>

#include <boost/system/error_code.hpp>

#include <spdlog/spdlog.h>

#include "ipv4_port.hpp"
#include "socket.hpp"
#include "tcp.hpp"

namespace mstack {

interceptor::interceptor(netns&                                                    net,
                         std::function<bool(ipv4_port_t const& remote_info,
                                            ipv4_port_t const& local_info)> const& matcher)
    : net_(net) {
        net_.tcp().rule_insert_back(matcher, tcp::PROTO, [this](tcp_packet&& pkt_in) {
                if (!cbs_.empty()) {
                        auto cb{cbs_.front()};
                        cbs_.pop();
                        return cb(std::move(pkt_in));
                }
                return false;
        });
}

interceptor::interceptor(netns& net, endpoint const& ep)
    : interceptor(net,
                  [ep](ipv4_port_t const& remote_info [[maybe_unused]],
                       ipv4_port_t const& local_info) { return local_info == ep.ep(); }) {}

interceptor::interceptor(endpoint const& ep) : interceptor(netns::_default_(), ep) {}

interceptor::~interceptor() = default;

netns& interceptor::net() { return net_; }
netns& interceptor::net() const { return net_; }

void interceptor::async_intercept(socket& sk, std::function<bool(tcp_packet&& pkt_in)> cb) {
        cbs_.push(std::move(cb));
}

}  // namespace mstack
