#include "ipv4.hpp"

#include <cassert>
#include <cstdint>

#include <utility>

#include <spdlog/spdlog.h>

#include "ethernetv2_frame.hpp"
#include "ipv4_header.hpp"
#include "ipv4_packet.hpp"
#include "mac_addr.hpp"
#include "routing_table.hpp"
#include "size_literals.hpp"
#include "tap.hpp"

namespace mstack {

ipv4::ipv4(boost::asio::io_context& io_ctx, std::shared_ptr<routing_table const> rt, arp& arp)
    : base_protocol(io_ctx), rt_(std::move(rt)), arp_(arp) {
        assert(rt_);
}

void ipv4::process(ipv4_packet&& in_pkt) {
        spdlog::debug("[IPV4] HDL FROM U-LAYER {}", in_pkt);

        ipv4_header_t ipv4_header = {
                .version       = 0x4,
                .header_length = 0x5,
                .total_length  = static_cast<uint16_t>(in_pkt.buffer->payload().size() +
                                                       ipv4_header_t::size()),
                .id            = seq_++,
                .ttl           = 0x40,
                .proto_type    = in_pkt.proto,
                .src_ip_addr   = in_pkt.src_ipv4_addr,
                .dst_ip_addr   = in_pkt.dst_ipv4_addr,
        };

        auto out_buffer{std::move(in_pkt.buffer)};

        out_buffer->push_front(ipv4_header_t::size());

        ipv4_header.produce_to_net(out_buffer->head());
        ipv4_header.header_chsum = utils::checksum_net({out_buffer->head(), ipv4_header_t::size()});
        ipv4_header.produce_to_net(out_buffer->head());

        ethernetv2_frame out_pkt{
                .proto  = PROTO,
                .buffer = std::move(out_buffer),
        };

        auto nh{rt_->query(in_pkt.dst_ipv4_addr)};
        if (!nh) nh = rt_->query_default();

        if (nh) {
                out_pkt.dev = std::move(nh->dev);
                assert(out_pkt.dev);
                out_pkt.src_mac_addr = out_pkt.dev->mac_addr();
                auto const& from_mac{out_pkt.src_mac_addr};
                auto const& from_ipv4{out_pkt.dev->ipv4_addr()};
                assert(from_ipv4);
                arp_.async_resolve(from_mac, *from_ipv4, nh->addr, out_pkt.dev,
                                   [this, out_packet_wrapper = std::make_shared<ethernetv2_frame>(
                                                  std::move(out_pkt))](mac_addr_t const& nh_addr) {
                                           auto out_packet{std::move(*out_packet_wrapper)};
                                           out_packet.dst_mac_addr = nh_addr;
                                           enqueue(std::move(out_packet));
                                   });
        } else {
                spdlog::error("[IPv4] NO NH for {}", in_pkt.dst_ipv4_addr);
        }
}

std::optional<ipv4_packet> ipv4::make_packet(ethernetv2_frame&& in_pkt) {
        spdlog::debug("[IPV4] HDL FROM L-LAYER {}", in_pkt);

        if (auto const* h{in_pkt.buffer->head()}; 0x4_b != ((h[0] >> 4) & 0xf_b)) return {};

        auto ipv4_header{ipv4_header_t::consume_from_net(in_pkt.buffer->head())};
        in_pkt.buffer->pop_front(ipv4_header_t::size());

        spdlog::debug("[RECEIVE] {}", ipv4_header);

        return ipv4_packet{
                .src_ipv4_addr = ipv4_header.src_ip_addr,
                .dst_ipv4_addr = ipv4_header.dst_ip_addr,
                .proto         = ipv4_header.proto_type,
                .buffer        = std::move(in_pkt.buffer),
        };
}

}  // namespace mstack
