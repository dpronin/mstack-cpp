#pragma once

#include "arp.hpp"
#include "base_protocol.hpp"
#include "ipv4_header.hpp"
#include "packets.hpp"

namespace mstack {

class ipv4 : public base_protocol<ethernetv2_packet, ipv4_packet, ipv4> {
public:
        uint16_t seq{0};

        constexpr static int PROTO{0x0800};

        std::optional<ethernetv2_packet> make_packet(ipv4_packet&& in_packet) override {
                spdlog::debug("[OUT] {}", in_packet);

                in_packet.buffer->reflush_packet(ipv4_header_t::size());

                ipv4_header_t out_ipv4_header = {
                        .version       = 0x4,
                        .header_length = 0x5,
                        .total_length  = static_cast<uint16_t>(in_packet.buffer->get_total_len() +
                                                               ipv4_header_t::size()),
                        .id            = seq++,
                        .ttl           = 0x40,
                        .proto_type    = static_cast<uint8_t>(in_packet.proto),
                        .src_ip_addr   = in_packet.src_ipv4_addr,
                        .dst_ip_addr   = in_packet.dst_ipv4_addr,
                };

                std::byte* pointer = in_packet.buffer->get_pointer();
                out_ipv4_header.produce(pointer);

                out_ipv4_header.header_checksum =
                        utils::checksum({pointer, ipv4_header_t::size()}, 0);

                out_ipv4_header.produce(pointer);

                ethernetv2_packet out_packet{
                        .proto  = PROTO,
                        .buffer = std::move(in_packet.buffer),
                };

                if (auto src_mac_addr{arp::instance().query_mac(in_packet.src_ipv4_addr)}) {
                        out_packet.src_mac_addr = *src_mac_addr;
                } else {
                        spdlog::warn("[NO MAC] {}", in_packet.src_ipv4_addr);
                }

                if (auto dst_mac_addr{arp::instance().query_mac(in_packet.dst_ipv4_addr)}) {
                        out_packet.dst_mac_addr = *dst_mac_addr;
                } else {
                        spdlog::warn("[NO MAC] {}", in_packet.dst_ipv4_addr);
                }

                return std::move(out_packet);
        }

        std::optional<ipv4_packet> make_packet(raw_packet&& in_packet) override {
                std::optional<ipv4_packet> r;

                auto* ptr{in_packet.buffer->get_pointer()};
                if (static_cast<std::byte>(0x4) != ((ptr[0] >> 4) & static_cast<std::byte>(0x0f)))
                        return r;

                auto ipv4_header{ipv4_header_t::consume(ptr)};
                in_packet.buffer->add_offset(ipv4_header_t::size());

                spdlog::debug("[RECEIVE] {}", ipv4_header);

                r = {
                        .src_ipv4_addr = ipv4_header.src_ip_addr,
                        .dst_ipv4_addr = ipv4_header.dst_ip_addr,
                        .proto         = ipv4_header.proto_type,
                        .buffer        = std::move(in_packet.buffer),
                };

                return r;
        }

        std::optional<ipv4_packet> make_packet(ethernetv2_packet&& in_packet) override {
                return make_packet(raw_packet{.buffer = std::move(in_packet.buffer)});
        };
};

}  // namespace mstack
