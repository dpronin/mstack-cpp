#pragma once

#include "tcp_packet.hpp"

namespace mstack {

class netns;

class raw_socket {
public:
        explicit raw_socket(netns& net, endpoint const& remote_ep, endpoint const& local_ep);

        ~raw_socket() = default;

        raw_socket(raw_socket const&)            = delete;
        raw_socket& operator=(raw_socket const&) = delete;

        raw_socket(raw_socket&&)            = delete;
        raw_socket& operator=(raw_socket&&) = delete;

        void attach();
        void detach();

        void async_read(::mstack::tcp_packet&                                 pkt,
                        std::function<void(boost::system::error_code const&)> cb);

        void async_write(::mstack::tcp_packet&&                                pkt,
                         std::function<void(boost::system::error_code const&)> cb);

        netns&       net() { return net_; }
        netns const& net() const { return net_; }

        endpoint const& remote_endpoint() const { return remote_ep_; }
        endpoint const& local_endpoint() const { return local_ep_; }

private:
        netns&   net_;
        endpoint remote_ep_;
        endpoint local_ep_;
};

}  // namespace mstack
