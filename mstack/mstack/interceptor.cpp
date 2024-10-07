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
        net_.tcp().rule_insert_back(matcher,
                                    [this](endpoint const& remote_ep, endpoint const& local_ep,
                                           std::shared_ptr<raw_socket::pqueue> rcv_pq) {
                                            if (!cbs_.empty()) {
                                                    auto cb{cbs_.front()};
                                                    cbs_.pop();
                                                    cb(remote_ep, local_ep, std::move(rcv_pq));
                                            }
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

void interceptor::async_intercept(std::function<void(std::unique_ptr<raw_socket> sk)> cb) {
        assert(cb);
        cbs_.push([this, cb = std::move(cb)](endpoint const& remote_ep, endpoint const& local_ep,
                                             std::shared_ptr<raw_socket::pqueue> rcv_pq) {
                cb(std::make_unique<raw_socket>(net_, remote_ep, local_ep, std::move(rcv_pq)));
        });
}

}  // namespace mstack
