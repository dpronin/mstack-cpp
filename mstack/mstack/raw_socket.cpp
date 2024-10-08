#include "raw_socket.hpp"

#include <cassert>
#include <utility>

#include "netns.hpp"
#include "tcp_packet.hpp"

namespace mstack {

raw_socket::raw_socket(netns& net, endpoint const& remote_ep, endpoint const& local_ep)
    : net_(net), remote_ep_(remote_ep), local_ep_(local_ep) {}

void raw_socket::attach() { net_.tcp().attach(remote_ep_, local_ep_); }
void raw_socket::detach() { net_.tcp().detach(remote_ep_, local_ep_); }

void raw_socket::async_read(::mstack::tcp_packet&                                 pkt,
                            std::function<void(boost::system::error_code const&)> cb) {
        net_.tcp().async_wait(remote_ep_, local_ep_,
                              [&pkt, cb = std::move(cb)](tcp_packet&& pkt_in) {
                                      pkt = std::move(pkt_in);
                                      cb({});
                              });
}

void raw_socket::async_write(::mstack::tcp_packet&&                                pkt,
                             std::function<void(boost::system::error_code const&)> cb) {
        net_.tcp().async_write(std::move(pkt), std::move(cb));
}

}  // namespace mstack
