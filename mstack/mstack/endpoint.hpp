#pragma once

#include "ipv4_addr.hpp"
#include "packets.hpp"

namespace mstack {

struct endpoint {
        ipv4_port_t ep;

        ipv4_addr_t address() const { return ep.ipv4_addr.value(); }
        port_addr_t port() const { return ep.port_addr.value(); }
};

}  // namespace mstack
