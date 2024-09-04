#pragma once

#include "arp.hpp"
#include "base_protocol.hpp"
#include "ipv4_header.hpp"
#include "packets.hpp"

namespace mstack {

class ipv4 : public base_protocol<ethernetv2_packet, ipv4_packet, ipv4> {
public:
        arp&                 arp_instance = arp::instance();
        int                  seq          = 0;
        constexpr static int PROTO        = 0x0800;

        int id() override { return PROTO; }

        std::optional<ethernetv2_packet> make_packet(ipv4_packet&& in_packet) override {
                SPDLOG_DEBUG("[OUT] {}", in_packet);

                in_packet.buffer->reflush_packet(ipv4_header_t::size());
                ipv4_header_t out_ipv4_header;

                out_ipv4_header.version       = 0x4;
                out_ipv4_header.header_length = 0x5;
                out_ipv4_header.total_length =
                        in_packet.buffer->get_total_len() + ipv4_header_t::size();
                out_ipv4_header.id          = seq++;
                out_ipv4_header.ttl         = 0x40;
                out_ipv4_header.proto_type  = in_packet.proto;
                out_ipv4_header.src_ip_addr = (in_packet.src_ipv4_addr).value();
                out_ipv4_header.dst_ip_addr = (in_packet.dst_ipv4_addr).value();

                uint8_t* pointer = reinterpret_cast<uint8_t*>(in_packet.buffer->get_pointer());
                out_ipv4_header.produce(pointer);
                uint16_t checksum = utils::checksum({pointer, ipv4_header_t::size()}, 0);
                out_ipv4_header.header_checksum = checksum;
                out_ipv4_header.produce(pointer);

                std::optional<mac_addr_t> src_mac_addr{
                        arp_instance.query_by_ipv4(in_packet.src_ipv4_addr.value()),
                };

                if (!src_mac_addr) SPDLOG_WARN("[NO MAC] {}", in_packet.src_ipv4_addr.value());

                std::optional<mac_addr_t> dst_mac_addr{
                        arp_instance.query_by_ipv4(in_packet.dst_ipv4_addr.value()),
                };

                if (!dst_mac_addr) SPDLOG_WARN("[NO MAC] {}", in_packet.dst_ipv4_addr.value());

                ethernetv2_packet out_packet{
                        .src_mac_addr = src_mac_addr,
                        .dst_mac_addr = dst_mac_addr,
                        .proto        = PROTO,
                        .buffer       = std::move(in_packet.buffer),
                };

                return std::move(out_packet);
        }

        std::optional<ipv4_packet> make_packet(raw_packet&& in_packet) override {
                std::optional<ipv4_packet> r;

                auto* ptr{reinterpret_cast<uint8_t*>(in_packet.buffer->get_pointer())};
                if (0x4 != ((ptr[0] >> 4) & 0x0f)) return r;

                auto ipv4_header{ipv4_header_t::consume(ptr)};
                in_packet.buffer->add_offset(ipv4_header_t::size());

                SPDLOG_DEBUG("[RECEIVE] {}", ipv4_header);

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
