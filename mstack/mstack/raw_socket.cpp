#include "raw_socket.hpp"

#include <memory>
#include <utility>

#include "netns.hpp"

namespace mstack {

raw_socket::raw_socket(netns&                  net,
                       endpoint const&         remote_ep,
                       endpoint const&         local_ep,
                       std::shared_ptr<pqueue> rcv_pq /* = {}*/)
    : net_(net), remote_ep_(remote_ep), local_ep_(local_ep), rcv_pq_(std::move(rcv_pq)) {}
}  // namespace mstack
