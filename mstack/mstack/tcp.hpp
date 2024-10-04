#pragma once

#include <cstdint>

#include <spdlog/spdlog.h>

#include "base_protocol.hpp"
#include "ipv4_packet.hpp"
#include "tcp_header.hpp"
#include "tcp_packet.hpp"

namespace mstack {

class tcp : public base_protocol<ipv4_packet, tcp_packet> {
public:
        constexpr static int PROTO{0x06};

        explicit tcp(boost::asio::io_context& io_ctx) : base_protocol(io_ctx) {}
        ~tcp() = default;

        tcp(tcp const&)            = delete;
        tcp& operator=(tcp const&) = delete;

        tcp(tcp&&)            = delete;
        tcp& operator=(tcp&&) = delete;

        void process(tcp_packet&& pkt_in) override {
                spdlog::debug("[TCP] HDL FROM U-LAYER {}", pkt_in);

                uint32_t const tcp_pseudo_header_chsum{
                        utils::hton(pkt_in.local_info.ipv4_addr.raw()) +
                                utils::hton(pkt_in.remote_info.ipv4_addr.raw()) +
                                utils::hton(static_cast<uint16_t>(pkt_in.proto)) +
                                utils::hton(static_cast<uint16_t>(pkt_in.skb.payload().size())),
                };

                auto tcp_header{tcp_header_t::consume_from_net(pkt_in.skb.head())};
                tcp_header.chsum =
                        utils::checksum_net(pkt_in.skb.payload(), tcp_pseudo_header_chsum);

                tcp_header.produce_to_net(pkt_in.skb.head());

                enqueue({
                        .src_ipv4_addr = pkt_in.local_info.ipv4_addr,
                        .dst_ipv4_addr = pkt_in.remote_info.ipv4_addr,
                        .proto         = pkt_in.proto,
                        .skb           = std::move(pkt_in.skb),
                });
        }

private:
        std::optional<tcp_packet> make_packet(ipv4_packet&& pkt_in) override {
                auto const tcp_header{
                        tcp_header_t::consume_from_net(pkt_in.skb.head()),
                };

                spdlog::debug("[RECEIVE] {}", tcp_header);

                return tcp_packet{
                        .proto = PROTO,
                        .remote_info =
                                {
                                        .ipv4_addr = pkt_in.src_ipv4_addr,
                                        .port_addr = tcp_header.src_port,
                                },
                        .local_info =
                                {
                                        .ipv4_addr = pkt_in.dst_ipv4_addr,
                                        .port_addr = tcp_header.dst_port,
                                },
                        .skb = std::move(pkt_in.skb),
                };
        }
};

}  // namespace mstack
