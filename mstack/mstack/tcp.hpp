#pragma once

#include "base_protocol.hpp"
#include "packets.hpp"
#include "tcp_header.hpp"

namespace mstack {

class tcp : public base_protocol<ipv4_packet, tcp_packet_t, tcp> {
public:
        constexpr static int PROTO{0x06};

        std::optional<ipv4_packet> make_packet(tcp_packet_t&& in_packet) override {
                uint32_t sum{0};

                sum += utils::ntoh(in_packet.local_info.ipv4_addr.raw());
                sum += utils::ntoh(in_packet.remote_info.ipv4_addr.raw());
                sum += utils::ntoh(in_packet.proto);
                sum += utils::ntoh(static_cast<uint16_t>(in_packet.buffer->get_remaining_len()));

                tcp_header_t tcp_header{tcp_header_t::consume(in_packet.buffer->get_pointer())};

                auto const checksum{
                        utils::checksum(
                                {
                                        in_packet.buffer->get_pointer(),
                                        static_cast<size_t>(in_packet.buffer->get_remaining_len()),
                                },
                                sum),
                };

                tcp_header.checksum = checksum;
                tcp_header.produce(in_packet.buffer->get_pointer());

                ipv4_packet out_ipv4{
                        .src_ipv4_addr = in_packet.local_info.ipv4_addr,
                        .dst_ipv4_addr = in_packet.remote_info.ipv4_addr,
                        .proto         = in_packet.proto,
                        .buffer        = std::move(in_packet.buffer),
                };

                return std::move(out_ipv4);
        }

        std::optional<tcp_packet_t> make_packet(ipv4_packet&& in_packet) override {
                auto const tcp_header{tcp_header_t::consume(in_packet.buffer->get_pointer())};

                spdlog::debug("[RECEIVE] {}", tcp_header);

                tcp_packet_t out_tcp_packet{
                        .proto = PROTO,
                        .remote_info =
                                {
                                        .ipv4_addr = in_packet.src_ipv4_addr,
                                        .port_addr = tcp_header.src_port,
                                },
                        .local_info =
                                {
                                        .ipv4_addr = in_packet.dst_ipv4_addr,
                                        .port_addr = tcp_header.dst_port,
                                },
                        .buffer = std::move(in_packet.buffer),
                };

                return std::move(out_tcp_packet);
        }
};

}  // namespace mstack
