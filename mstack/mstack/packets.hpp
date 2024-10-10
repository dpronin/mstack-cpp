#pragma once

#include <cstddef>

#include <boost/container_hash/hash.hpp>

#include "endpoint.hpp"

namespace mstack {

struct two_ends_t {
        endpoint remote_ep;
        endpoint local_ep;

        auto operator<=>(two_ends_t const& other) const = default;
};

}  // namespace mstack

namespace std {

template <>
struct hash<mstack::two_ends_t> {
        size_t operator()(const mstack::two_ends_t& two_ends) const {
                size_t seed{0};
                boost::hash_combine(seed, two_ends.remote_ep);
                boost::hash_combine(seed, two_ends.local_ep);
                return seed;
        }
};

}  // namespace std

namespace mstack {
inline size_t hash_value(two_ends_t const& v) { return std::hash<two_ends_t>{}(v); }
}  // namespace mstack
