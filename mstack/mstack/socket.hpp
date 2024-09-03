#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>

#include "circle_buffer.hpp"
#include "defination.hpp"
#include "packets.hpp"

namespace mstack {

struct tcb_t;

struct socket_t {
        int                        fd;
        int                        state = SOCKET_UNCONNECTED;
        int                        proto;
        std::optional<ipv4_port_t> local_info;
        std::optional<ipv4_port_t> remote_info;
        std::shared_ptr<tcb_t>     tcb;
};

struct listener_t {
        listener_t() : acceptors(std::make_shared<circle_buffer<std::shared_ptr<tcb_t>>>()) {}
        int                                                    fd;
        int                                                    state = SOCKET_UNCONNECTED;
        int                                                    proto;
        mutable std::mutex                                     lock;
        std::condition_variable                                cv;
        std::shared_ptr<circle_buffer<std::shared_ptr<tcb_t>>> acceptors;
        std::optional<ipv4_port_t>                             local_info;
};

}  // namespace mstack
