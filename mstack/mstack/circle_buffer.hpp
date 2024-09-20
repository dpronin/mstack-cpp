#pragma once

#include <optional>
#include <queue>

namespace mstack {

template <typename PacketType>
class circle_buffer {
private:
        std::queue<PacketType> packets;

public:
        circle_buffer()  = default;
        ~circle_buffer() = default;

        circle_buffer(circle_buffer const&)            = default;
        circle_buffer& operator=(circle_buffer const&) = default;

        circle_buffer(circle_buffer&&)            = default;
        circle_buffer& operator=(circle_buffer&&) = default;

        bool empty() const { return packets.size() == 0; }
        void push(PacketType packet) { packets.push(std::move(packet)); }

        size_t size() const { return packets.size(); }

        std::optional<PacketType> pop() {
                std::optional<PacketType> r;
                if (!empty()) {
                        r = std::move(packets.front());
                        packets.pop();
                }
                return r;
        }
};

}  // namespace mstack
