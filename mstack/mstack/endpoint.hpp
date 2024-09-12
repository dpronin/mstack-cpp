#pragma once

#include "ipv4_addr.hpp"
#include "packets.hpp"

namespace mstack {

class endpoint {
public:
        endpoint(ipv4_port_t const& ep) : ep_(ep) {}

        endpoint(endpoint const&)            = default;
        endpoint& operator=(endpoint const&) = default;

        endpoint(endpoint&&)            = default;
        endpoint& operator=(endpoint&&) = default;

        ipv4_addr_t address() const { return ep_.ipv4_addr; }
        port_addr_t port() const { return ep_.port_addr; }

private:
        ipv4_port_t ep_;
};

}  // namespace mstack
