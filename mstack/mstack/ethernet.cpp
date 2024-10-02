#include "ethernet.hpp"

#include <cassert>
#include <utility>

#include <spdlog/spdlog.h>

#include "ethernet_header.hpp"

#include "tap.hpp"

namespace mstack {

ethernetv2::ethernetv2(boost::asio::io_context& io_ctx) : base_protocol(io_ctx) {}

void ethernetv2::process(ethernetv2_frame&& in_frame) {
        assert(in_frame.dev);

        spdlog::debug("[ETH] HDL FROM U-LAYER {} from dev {}", in_frame, in_frame.dev->name());

        auto const e_packet = ethernetv2_header_t{
                .dst_mac_addr = in_frame.dst_mac_addr,
                .src_mac_addr = in_frame.src_mac_addr,
                .proto        = in_frame.proto,
        };

        auto out_buffer{std::move(in_frame.buffer)};
        e_packet.produce_to_net(out_buffer->push_front(ethernetv2_header_t::size()));

        enqueue({.buffer = std::move(out_buffer)}, std::move(in_frame.dev));
}

std::optional<ethernetv2_frame> ethernetv2::make_packet(raw_packet&&         in_pkt,
                                                        std::shared_ptr<tap> dev) {
        auto const e_header{ethernetv2_header_t::consume_from_net(in_pkt.buffer->head())};
        in_pkt.buffer->pop_front(ethernetv2_header_t::size());

        return ethernetv2_frame{
                .src_mac_addr = e_header.src_mac_addr,
                .dst_mac_addr = e_header.dst_mac_addr,
                .proto        = e_header.proto,
                .buffer       = std::move(in_pkt.buffer),
                .dev          = std::move(dev),
        };
}

}  // namespace mstack
