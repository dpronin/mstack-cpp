#pragma once

#include <optional>

#include "base_protocol.hpp"
#include "ethernet_header.hpp"
#include "ethernetv2_frame.hpp"
#include "raw_packet.hpp"

namespace mstack {

class ethernetv2 : public base_protocol<raw_packet, ethernetv2_frame, ethernetv2> {
public:
        std::optional<raw_packet> make_packet(ethernetv2_frame&& in_packet) override {
                spdlog::debug("[OUT] {}", in_packet);

                ethernetv2_header_t e_packet = {
                        .dst_mac_addr = in_packet.dst_mac_addr,
                        .src_mac_addr = in_packet.src_mac_addr,
                        .proto        = in_packet.proto,
                };

                in_packet.buffer->reflush_packet(ethernetv2_header_t::size());
                e_packet.produce(in_packet.buffer->get_pointer());

                return raw_packet{.buffer = std::move(in_packet.buffer)};
        }

        std::optional<ethernetv2_frame> make_packet(raw_packet&& in_packet) override {
                auto e_header{ethernetv2_header_t::consume(in_packet.buffer->get_pointer())};

                in_packet.buffer->add_offset(ethernetv2_header_t::size());

                return ethernetv2_frame{
                        .src_mac_addr = e_header.src_mac_addr,
                        .dst_mac_addr = e_header.dst_mac_addr,
                        .proto        = e_header.proto,
                        .buffer       = std::move(in_packet.buffer),
                };
        }
};

}  // namespace mstack
