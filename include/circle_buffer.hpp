#pragma once

#include <optional>
#include <queue>

namespace mstack {

template <typename PacketType>
class circle_buffer {
private:
        std::queue<PacketType> packets;

public:
        bool empty() const { return packets.size() == 0; }
        void push_back(PacketType packet) { packets.push(std::move(packet)); }

        size_t size() const { return packets.size(); }

        std::optional<PacketType> pop_front() {
                std::optional<PacketType> r;
                if (!empty()) {
                        r = std::move(packets.front());
                        packets.pop();
                }
                return r;
        }
};

}  // namespace mstack
