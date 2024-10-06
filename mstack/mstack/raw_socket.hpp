#pragma once

#include <queue>

#include "tcp_packet.hpp"

namespace mstack {

class netns;

class raw_socket {
public:
        using pqueue = std::queue<::mstack::tcp_packet>;

        explicit raw_socket(netns& net) : net_(net) {}
        raw_socket();

        ~raw_socket() = default;

        raw_socket(raw_socket const&)            = delete;
        raw_socket& operator=(raw_socket const&) = delete;

        raw_socket(raw_socket&&)            = delete;
        raw_socket& operator=(raw_socket&&) = delete;

        netns&       net() { return net_; }
        netns const& net() const { return net_; }

        pqueue&       packets() { return packets_; }
        pqueue const& packets() const { return packets_; }

private:
        netns& net_;
        pqueue packets_;
};

}  // namespace mstack
