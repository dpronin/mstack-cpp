#pragma once

#include <algorithm>

#include "base_protocol.hpp"
#include "defination.hpp"
#include "ethernet_header.hpp"
#include "mac_addr.hpp"
#include "packets.hpp"

namespace mstack {

class ethernetv2 : public base_protocol<raw_packet, ethernetv2_packet, ethernetv2> {
public:
        int id() override { return 0; }

        std::optional<raw_packet> make_packet(ethernetv2_packet&& in_packet) override {
                if (!in_packet.dst_mac_addr || !in_packet.src_mac_addr) {
                        return std::nullopt;
                }
                SPDLOG_DEBUG("[OUT] {}", in_packet);
                ethernetv2_header_t e_packet;
                e_packet.dst_mac_addr = in_packet.dst_mac_addr.value();
                e_packet.src_mac_addr = in_packet.src_mac_addr.value();
                e_packet.proto        = in_packet.proto;
                in_packet.buffer->reflush_packet(ethernetv2_header_t::size());
                e_packet.produce(reinterpret_cast<uint8_t*>(in_packet.buffer->get_pointer()));
                raw_packet out_packet = {.buffer = std::move(in_packet.buffer)};
                return std::move(out_packet);
        }

        std::optional<ethernetv2_packet> make_packet(raw_packet&& in_packet) override {
                auto e_header = ethernetv2_header_t::consume(
                        reinterpret_cast<uint8_t*>(in_packet.buffer->get_pointer()));
                in_packet.buffer->add_offset(ethernetv2_header_t::size());
                ethernetv2_packet out_packet = {.src_mac_addr = e_header.src_mac_addr,
                                                .dst_mac_addr = e_header.dst_mac_addr,
                                                .proto        = e_header.proto,
                                                .buffer       = std::move(in_packet.buffer)};
                return std::move(out_packet);
        }
};

}  // namespace mstack
