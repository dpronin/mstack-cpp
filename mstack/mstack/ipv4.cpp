#include "ipv4.hpp"

#include <cassert>
#include <cstdint>

#include <utility>

#include <spdlog/spdlog.h>

#include "device.hpp"
#include "ethernetv2_frame.hpp"
#include "ipv4_header.hpp"
#include "ipv4_packet.hpp"
#include "mac_addr.hpp"
#include "mstack/arp_cache.hpp"
#include "routing_table.hpp"
#include "size_literals.hpp"

namespace mstack {

ipv4::ipv4(boost::asio::io_context&             io_ctx,
           std::shared_ptr<routing_table const> rt,
           std::shared_ptr<arp_cache_t const>   arp_cache,
           arp&                                 arp)
    : base_protocol(io_ctx), rt_(std::move(rt)), arp_cache_(std::move(arp_cache)), arp_(arp) {
        assert(rt_);
        assert(arp_cache_);
}

void ipv4::process(ipv4_packet&& pkt_in) {
        spdlog::debug("[IPV4] HDL FROM U-LAYER {}", pkt_in);

        auto ipv4_header = ipv4_header_t{
                .version       = 0x4,
                .header_length = 0x5,
                .tos           = 0x0,
                .total_length =
                        static_cast<uint16_t>(pkt_in.skb.payload().size() + ipv4_header_t::size()),
                .id           = seq_++,
                .NOP          = 0,
                .DF           = 0,
                .MF           = 0,
                .frag_offset  = 0,
                .ttl          = 0x40,
                .proto_type   = pkt_in.proto,
                .header_chsum = 0,
                .src_addr     = pkt_in.src_addrv4,
                .dst_addr     = pkt_in.dst_addrv4,
        };

        auto out_buffer{std::move(pkt_in.skb)};

        out_buffer.push_front(ipv4_header_t::size());

        ipv4_header.produce_to_net(out_buffer.head());
        ipv4_header.header_chsum = utils::checksum_net({out_buffer.head(), ipv4_header_t::size()});
        ipv4_header.produce_to_net(out_buffer.head());

        auto out_pkt = ethernetv2_frame{
                .src_mac_addr = {},
                .dst_mac_addr = {},
                .proto        = PROTO,
                .skb          = std::move(out_buffer),
                .dev          = {},
        };

        auto nh{rt_->query(pkt_in.dst_addrv4)};
        if (!nh) nh = rt_->query_default();

        if (nh) {
                out_pkt.dev = std::move(nh->dev);
                assert(out_pkt.dev);

                auto const src_mac_opt{arp_cache_->query(nh->from_addrv4)};
                assert(src_mac_opt);
                out_pkt.src_mac_addr = *src_mac_opt;

                arp_.async_resolve(out_pkt.src_mac_addr, nh->from_addrv4, nh->via_addrv4,
                                   out_pkt.dev,
                                   [this, out_packet_wrapper = std::make_shared<ethernetv2_frame>(
                                                  std::move(out_pkt))](mac_addr_t const& nh_addr) {
                                           auto out_packet{std::move(*out_packet_wrapper)};
                                           out_packet.dst_mac_addr = nh_addr;
                                           enqueue(std::move(out_packet));
                                   });
        } else {
                spdlog::error("[IPv4] NO NH for {}", pkt_in.dst_addrv4);
        }
}

std::optional<ipv4_packet> ipv4::make_packet(ethernetv2_frame&& frame_in) {
        spdlog::debug("[IPV4] HDL FROM L-LAYER {}", frame_in);

        if (auto const* h{frame_in.skb.head()}; 0x4_b != ((h[0] >> 4) & 0xf_b)) return {};

        auto ipv4_header{ipv4_header_t::consume_from_net(frame_in.skb.head())};
        frame_in.skb.pop_front(ipv4_header_t::size());

        spdlog::debug("[IPv4] RECEIVE {}", ipv4_header);

        return ipv4_packet{
                .src_addrv4 = ipv4_header.src_addr,
                .dst_addrv4 = ipv4_header.dst_addr,
                .proto      = ipv4_header.proto_type,
                .skb        = std::move(frame_in.skb),
        };
}

}  // namespace mstack
