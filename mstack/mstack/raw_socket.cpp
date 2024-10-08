#include "raw_socket.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "netns.hpp"

namespace mstack {

raw_socket::raw_socket(netns&                  net,
                       endpoint const&         remote_ep,
                       endpoint const&         local_ep,
                       std::shared_ptr<pqueue> rcv_pq /* = {}*/)
    : net_(net), remote_ep_(remote_ep), local_ep_(local_ep), rcv_pq_(std::move(rcv_pq)) {}

void raw_socket::attach() { throw std::runtime_error{"function is not implemented"}; }

void raw_socket::async_read_some(std::span<std::byte> buf [[maybe_unused]],
                                 std::function<void(boost::system::error_code const&, size_t)> cb
                                 [[maybe_unused]]) {
        throw std::runtime_error{"function is not implemented"};
}

void raw_socket::async_read(std::span<std::byte> buf [[maybe_unused]],
                            std::function<void(boost::system::error_code const&, size_t)> cb
                            [[maybe_unused]]) {
        throw std::runtime_error{"function is not implemented"};
}

void raw_socket::async_write_some(std::span<std::byte const> buf [[maybe_unused]],
                                  std::function<void(boost::system::error_code const&, size_t)> cb
                                  [[maybe_unused]]) {
        throw std::runtime_error{"function is not implemented"};
}

void raw_socket::async_write(std::span<std::byte const> buf [[maybe_unused]],
                             std::function<void(boost::system::error_code const&, size_t)> cb
                             [[maybe_unused]]) {
        throw std::runtime_error{"function is not implemented"};
}

}  // namespace mstack
