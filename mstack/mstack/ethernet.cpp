#include "ethernet.hpp"

#include <cassert>
#include <utility>

#include <spdlog/spdlog.h>

#include "ethernet_header.hpp"

#include "tap.hpp"

namespace mstack {

ethernetv2::ethernetv2(boost::asio::io_context& io_ctx) : base_protocol(io_ctx) {}

void ethernetv2::process(ethernetv2_frame&& in_pkt) {
        assert(in_pkt.dev);

        spdlog::debug("[ETH] HDL FROM U-LAYER {} from dev {}", in_pkt, in_pkt.dev->name());

        ethernetv2_header_t const e_packet = {
                .dst_mac_addr = in_pkt.dst_mac_addr,
                .src_mac_addr = in_pkt.src_mac_addr,
                .proto        = in_pkt.proto,
        };

        in_pkt.buffer->reflush_packet(ethernetv2_header_t::size());
        e_packet.produce_to_net(in_pkt.buffer->get_pointer());

        enqueue({.buffer = std::move(in_pkt.buffer)}, std::move(in_pkt.dev));
}

std::optional<ethernetv2_frame> ethernetv2::make_packet(raw_packet&&         in_pkt,
                                                        std::shared_ptr<tap> dev) {
        auto const e_header{
                ethernetv2_header_t::consume_from_net(in_pkt.buffer->get_pointer()),
        };

        in_pkt.buffer->add_offset(ethernetv2_header_t::size());

        return ethernetv2_frame{
                .src_mac_addr = e_header.src_mac_addr,
                .dst_mac_addr = e_header.dst_mac_addr,
                .proto        = e_header.proto,
                .buffer       = std::move(in_pkt.buffer),
                .dev          = std::move(dev),
        };
}

}  // namespace mstack
