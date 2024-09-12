#pragma once

#include <cstddef>

#include <span>
#include <utility>

#include <boost/system/error_code.hpp>

#include "socket.hpp"

namespace mstack {

inline void async_write(socket_t&                                                     sk,
                        std::span<std::byte const>                                    buf,
                        std::function<void(boost::system::error_code const&, size_t)> cb) {
        sk.async_write(buf, std::move(cb));
}

}  // namespace mstack
