#pragma once

#include <boost/asio/io_context.hpp>

#include "base_protocol.hpp"
#include "ipv4_packet.hpp"
#include "tcp_packet.hpp"

namespace mstack {

class tcp : public base_protocol<ipv4_packet, tcp_packet> {
public:
        constexpr static int PROTO{0x06};

        explicit tcp(boost::asio::io_context& io_ctx);
        ~tcp() = default;

        tcp(tcp const&)            = delete;
        tcp& operator=(tcp const&) = delete;

        tcp(tcp&&)            = delete;
        tcp& operator=(tcp&&) = delete;

        void process(tcp_packet&& pkt_in) override;

private:
        std::optional<tcp_packet> make_packet(ipv4_packet&& pkt_in) override;
};

}  // namespace mstack
