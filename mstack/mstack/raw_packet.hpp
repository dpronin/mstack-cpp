#pragma once

#include <memory>

#include "base_packet.hpp"

namespace mstack {

struct raw_packet {
        std::unique_ptr<base_packet> buffer;
};

}  // namespace mstack
