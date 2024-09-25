#pragma once

#include "ipv4_port.hpp"

#include <boost/container_hash/hash.hpp>

namespace mstack {

class endpoint {
public:
        endpoint() = default;
        endpoint(int proto, ipv4_port_t const& ep) : proto_(proto), ep_(ep) {}

        endpoint(endpoint const&)            = default;
        endpoint& operator=(endpoint const&) = default;

        endpoint(endpoint&&)            = default;
        endpoint& operator=(endpoint&&) = default;

        auto operator<=>(endpoint const& other) const = default;

        int                proto() const { return proto_; }
        ipv4_port_t const& ep() const { return ep_; }

private:
        int         proto_{-1};
        ipv4_port_t ep_;
};

}  // namespace mstack

namespace std {
template <>
struct hash<mstack::endpoint> {
        size_t operator()(mstack::endpoint const& ep) const {
                auto seed{0uz};
                boost::hash_combine(seed, ep.proto());
                boost::hash_combine(seed, ep.ep());
                return seed;
        }
};

}  // namespace std

namespace mstack {
inline size_t hash_value(endpoint const& v) { return std::hash<endpoint>{}(v); }
}  // namespace mstack
