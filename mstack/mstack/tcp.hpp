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

        void process(tcp_packet&& in_pkt) override {
                spdlog::debug("[TCP] HDL FROM U-LAYER {}", in_pkt);

                uint32_t const tcp_pseudo_header_chsum{
                        utils::hton(in_pkt.local_info.ipv4_addr.raw()) +
                                utils::hton(in_pkt.remote_info.ipv4_addr.raw()) +
                                utils::hton(static_cast<uint16_t>(in_pkt.proto)) +
                                utils::hton(static_cast<uint16_t>(in_pkt.buffer->payload().size())),
                };

                auto tcp_header{tcp_header_t::consume_from_net(in_pkt.buffer->head())};
                tcp_header.chsum =
                        utils::checksum_net(in_pkt.buffer->payload(), tcp_pseudo_header_chsum);

                tcp_header.produce_to_net(in_pkt.buffer->head());

                enqueue({
                        .src_ipv4_addr = in_pkt.local_info.ipv4_addr,
                        .dst_ipv4_addr = in_pkt.remote_info.ipv4_addr,
                        .proto         = in_pkt.proto,
                        .buffer        = std::move(in_pkt.buffer),
                });
        }

private:
        std::optional<tcp_packet> make_packet(ipv4_packet&& in_pkt) override {
                auto const tcp_header{
                        tcp_header_t::consume_from_net(in_pkt.buffer->head()),
                };

                spdlog::debug("[RECEIVE] {}", tcp_header);

                return tcp_packet{
                        .proto = PROTO,
                        .remote_info =
                                {
                                        .ipv4_addr = in_pkt.src_ipv4_addr,
                                        .port_addr = tcp_header.src_port,
                                },
                        .local_info =
                                {
                                        .ipv4_addr = in_pkt.dst_ipv4_addr,
                                        .port_addr = tcp_header.dst_port,
                                },
                        .buffer = std::move(in_pkt.buffer),
                };
        }
};

}  // namespace mstack
