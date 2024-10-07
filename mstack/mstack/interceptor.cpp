#include "interceptor.hpp"

#include <cassert>

#include <utility>

#include <boost/system/error_code.hpp>

#include <spdlog/spdlog.h>

#include "endpoint.hpp"
#include "raw_socket.hpp"
#include "tcp.hpp"

namespace mstack {

interceptor::interceptor(
        netns&                                                                          net,
        std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> const& matcher)
    : net_(net) {
        net_.tcp().rule_insert_back(matcher, tcp::PROTO, [this](tcp_packet const& pkt_in) {
                if (!cbs_.empty()) {
                        auto cb{cbs_.front()};
                        cbs_.pop();
                        return cb(std::move(pkt_in));
                }
                return false;
        });
}

interceptor::interceptor(netns& net, endpoint const& local_ep)
    : interceptor(net,
                  [local_ep_exp = local_ep](endpoint const& remote_ep [[maybe_unused]],
                                            endpoint const& local_ep) {
                          return local_ep_exp == local_ep;
                  }) {}

interceptor::interceptor(endpoint const& local_ep) : interceptor(netns::_default_(), local_ep) {}

interceptor::~interceptor() = default;

netns& interceptor::net() { return net_; }
netns& interceptor::net() const { return net_; }

void interceptor::async_intercept(raw_socket&                                   sk,
                                  std::function<bool(tcp_packet const& pkt_in)> cb) {
        cbs_.push(std::move(cb));
}

}  // namespace mstack
