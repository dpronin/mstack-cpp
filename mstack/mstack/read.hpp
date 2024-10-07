#pragma once

#include <cstddef>

#include <span>
#include <utility>

#include <boost/system/error_code.hpp>

#include "raw_socket.hpp"
#include "socket.hpp"

namespace mstack {

inline void async_read(socket&                                                       sk,
                       std::span<std::byte>                                          buf,
                       std::function<void(boost::system::error_code const&, size_t)> cb) {
        sk.async_read(buf, std::move(cb));
}

inline void async_read(raw_socket&                                                   sk,
                       std::span<std::byte>                                          buf,
                       std::function<void(boost::system::error_code const&, size_t)> cb) {
        sk.async_read(buf, std::move(cb));
}

}  // namespace mstack
