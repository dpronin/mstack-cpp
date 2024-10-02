#include "ethernet.hpp"

#include <utility>

#include <spdlog/spdlog.h>

#include "ethernet_header.hpp"

namespace mstack {

ethernetv2::ethernetv2(boost::asio::io_context& io_ctx) : base_protocol(io_ctx) {}

void ethernetv2::process(ethernetv2_frame&& in_packet) {
        spdlog::debug("[ETH] HDL FROM U-LAYER {}", in_packet);

        ethernetv2_header_t const e_packet = {
                .dst_mac_addr = in_packet.dst_mac_addr,
                .src_mac_addr = in_packet.src_mac_addr,
                .proto        = in_packet.proto,
        };

        in_packet.buffer->reflush_packet(ethernetv2_header_t::size());
        e_packet.produce_to_net(in_packet.buffer->get_pointer());

        enqueue({.buffer = std::move(in_packet.buffer)});
}

std::optional<ethernetv2_frame> ethernetv2::make_packet(raw_packet&& in_packet) {
        auto const e_header{
                ethernetv2_header_t::consume_from_net(in_packet.buffer->get_pointer()),
        };

        in_packet.buffer->add_offset(ethernetv2_header_t::size());

        return ethernetv2_frame{
                .src_mac_addr = e_header.src_mac_addr,
                .dst_mac_addr = e_header.dst_mac_addr,
                .proto        = e_header.proto,
                .buffer       = std::move(in_packet.buffer),
        };
}

}  // namespace mstack
