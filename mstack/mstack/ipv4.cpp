#include "ipv4.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <utility>

#include <spdlog/spdlog.h>

#include "device.hpp"
#include "ethernetv2_frame.hpp"
#include "ipv4_header.hpp"
#include "ipv4_packet.hpp"
#include "mac_addr.hpp"
#include "mstack/neigh_cache.hpp"
#include "routing_table.hpp"
#include "size_literals.hpp"

namespace mstack {

ipv4::ipv4(boost::asio::io_context&             io_ctx,
           std::shared_ptr<routing_table const> rt,
           std::shared_ptr<neigh_cache const>   arp_cache,
           arp&                                 arp)
    : base_protocol(io_ctx), rt_(std::move(rt)), arp_cache_(std::move(arp_cache)), arp_(arp) {
        assert(rt_);
        assert(arp_cache_);
}

void ipv4::process(ipv4_packet&& pkt_in) {
        spdlog::debug("[IPV4] HDL FROM U-LAYER {}", pkt_in);

        auto const ipv4h = ipv4_header_t{
                .version      = 0x4,
                .hlen         = 0x5,
                .tos          = 0x0,
                .total_length = static_cast<uint16_t>(pkt_in.skb.payload().size() +
                                                      ipv4_header_t::fixed_size()),
                .id           = seq_++,
                .NOP          = 0,
                .DF           = 1,
                .MF           = 0,
                .frag_offset  = 0,
                .ttl          = 0x40,
                .proto_type   = pkt_in.proto,
                .header_chsum = 0,
                .src_addr     = pkt_in.src_addrv4,
                .dst_addr     = pkt_in.dst_addrv4,
        };

        auto skb_out{std::move(pkt_in.skb)};

        assert(!(skb_out.headroom() < ipv4_header_t::fixed_size()));
        skb_out.push_front(ipv4_header_t::fixed_size());
        ipv4h.produce_to_net(skb_out.head());

        uint16_t const header_chsum_net{
                utils::checksum({skb_out.head(), ipv4_header_t::fixed_size()}),
        };
        std::memcpy(skb_out.head() + offsetof(ipv4_header_t, header_chsum), &header_chsum_net,
                    sizeof(header_chsum_net));

        auto out_frame = ethernetv2_frame{
                .src_mac_addr = {},
                .dst_mac_addr = {},
                .proto        = PROTO,
                .skb          = std::move(skb_out),
                .dev          = {},
        };

        auto nh{rt_->query(pkt_in.dst_addrv4)};
        if (!nh) nh = rt_->query_default();

        if (nh) {
                out_frame.dev = std::move(nh->dev);
                assert(out_frame.dev);

                auto const src_mac_opt{arp_cache_->query(nh->from_addrv4)};
                assert(src_mac_opt);
                out_frame.src_mac_addr = *src_mac_opt;

                auto const& src_mac{out_frame.src_mac_addr};
                auto        dev{out_frame.dev};
                arp_.async_resolve(
                        src_mac, nh->from_addrv4, nh->via_addrv4, dev,
                        [this, out_frame_wrapper = std::make_shared<ethernetv2_frame>(
                                       std::move(out_frame))](mac_addr_t const& nh_addr) {
                                auto out_packet{std::move(*out_frame_wrapper)};
                                out_packet.dst_mac_addr = nh_addr;
                                enqueue(std::move(out_packet));
                        });
        } else {
                spdlog::error("[IPv4] NO NH for {}", pkt_in.dst_addrv4);
        }
}

std::optional<ipv4_packet> ipv4::make_packet(ethernetv2_frame&& frame_in) {
        spdlog::debug("[IPV4] HDL FROM L-LAYER {}", frame_in);

        assert(!(frame_in.skb.payload().size() < ipv4_header_t::fixed_size()));

        if (auto const fb{frame_in.skb.payload()[0]}; 0x4_b != ((fb >> 4) & 0xf_b)) return {};

        auto const ipv4_header{ipv4_header_t::consume_from_net(frame_in.skb.head())};
        auto const hlen{static_cast<size_t>(ipv4_header.hlen << 2)};
        assert(!(hlen < ipv4_header_t::fixed_size()));
        frame_in.skb.pop_front(hlen);

        spdlog::debug("[IPv4] RECEIVE {}", ipv4_header);

        return ipv4_packet{
                .src_addrv4 = ipv4_header.src_addr,
                .dst_addrv4 = ipv4_header.dst_addr,
                .proto      = ipv4_header.proto_type,
                .skb        = std::move(frame_in.skb),
        };
}

}  // namespace mstack
