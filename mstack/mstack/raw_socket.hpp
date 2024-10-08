#pragma once

#include <memory>
#include <queue>

#include "tcp_packet.hpp"

namespace mstack {

class netns;

class raw_socket {
public:
        using pqueue = std::queue<::mstack::tcp_packet>;

        explicit raw_socket(netns&                  net,
                            endpoint const&         remote_ep,
                            endpoint const&         local_ep,
                            std::shared_ptr<pqueue> rcv_pq = {});

        ~raw_socket() = default;

        raw_socket(raw_socket const&)            = delete;
        raw_socket& operator=(raw_socket const&) = delete;

        raw_socket(raw_socket&&)            = delete;
        raw_socket& operator=(raw_socket&&) = delete;

        void attach();
        void detach();

        void async_read_some(std::span<std::byte>                                          buf,
                             std::function<void(boost::system::error_code const&, size_t)> cb);

        void async_read(std::span<std::byte>                                          buf,
                        std::function<void(boost::system::error_code const&, size_t)> cb);

        void async_write_some(std::span<std::byte const>                                    buf,
                              std::function<void(boost::system::error_code const&, size_t)> cb);

        void async_write(std::span<std::byte const>                                    buf,
                         std::function<void(boost::system::error_code const&, size_t)> cb);

        netns&       net() { return net_; }
        netns const& net() const { return net_; }

        endpoint const& remote_endpoint() const { return remote_ep_; }
        endpoint const& local_endpoint() const { return local_ep_; }

private:
        netns&   net_;
        endpoint remote_ep_;
        endpoint local_ep_;

        std::shared_ptr<pqueue> rcv_pq_;
};

}  // namespace mstack
