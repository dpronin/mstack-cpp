#pragma once

#include <cstddef>

#include <span>
#include <utility>

#include <boost/system/error_code.hpp>

#include "raw_socket.hpp"
#include "socket.hpp"

namespace mstack {

inline void async_write(socket&                                                       sk,
                        std::span<std::byte const>                                    buf,
                        std::function<void(boost::system::error_code const&, size_t)> cb) {
        sk.async_write(buf, std::move(cb));
}

inline void async_write(raw_socket&                                                   sk,
                        std::span<std::byte const>                                    buf,
                        std::function<void(boost::system::error_code const&, size_t)> cb) {
        sk.async_write(buf, std::move(cb));
}

}  // namespace mstack
