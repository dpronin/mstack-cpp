#include "arp.hpp"

#include <cassert>
#include <cstddef>

#include <utility>

#include <spdlog/spdlog.h>

#include "arp_header.hpp"
#include "ethernet_header.hpp"

namespace {
std::byte operator""_b(unsigned long long x) { return static_cast<std::byte>(x); }
}  // namespace

namespace mstack {

arp::arp(boost::asio::io_context& io_ctx, std::shared_ptr<neigh_cache> arp_cache)
    : base_protocol(io_ctx), arp_cache_(std::move(arp_cache)) {
        assert(arp_cache_);
}

void arp::async_resolve(mac_addr_t const&                          from_mac,
                        ipv4_addr_t const&                         from_ipv4,
                        ipv4_addr_t const&                         to_ipv4,
                        std::shared_ptr<device>                    dev,
                        std::function<void(mac_addr_t const& mac)> cb) {
        if (auto const mac{arp_cache_->query(to_ipv4)}) {
                io_ctx_.post([cb = std::move(cb), mac = *mac]() mutable { cb(mac); });
        } else {
                on_replies_[to_ipv4].connect(cb);
                async_request({from_mac, from_ipv4}, to_ipv4, std::move(dev));
        }
}

void arp::update(std::pair<mac_addr_t, ipv4_addr_t> const& peer) {
        if (!(peer.first.is_broadcast() || peer.first.is_zero())) {
                spdlog::debug("[ARP] UPDATE CACHE {} -> {}", peer.first, peer.second);
                arp_cache_->update({peer.second, peer.first});
        }
}

void arp::async_reply(std::pair<mac_addr_t, ipv4_addr_t> const& from,
                      std::pair<mac_addr_t, ipv4_addr_t> const& to,
                      ethernetv2_frame&&                        in_frame) {
        arpv4_header_t const out_arp{
                .htype = 0x0001u,
                .ptype = 0x0800u,
                .hlen  = 0x06u,
                .plen  = 0x04u,
                .oper  = 0x0002u,
                .sha   = from.first,
                .spa   = from.second,
                .tha   = to.first,
                .tpa   = to.second,
        };

        auto out_buffer{std::move(in_frame.skb)};

        out_buffer.push_front(arpv4_header_t::size());
        out_arp.produce_to_net(out_buffer.head());

        spdlog::debug("[ARP] ENQUEUE REPLY {}", out_arp);

        enqueue({
                .src_mac_addr = out_arp.sha,
                .dst_mac_addr = out_arp.tha,
                .proto        = PROTO,
                .skb          = std::move(out_buffer),
                .dev          = std::move(in_frame.dev),
        });
}

void arp::process_request(arpv4_header_t const& in_arp, ethernetv2_frame&& in_frame) {
        spdlog::debug("[ARP] PROCESS REQ");
        update({in_arp.sha, in_arp.spa});
        if (auto const& tha{arp_cache_->query(in_arp.tpa)})
                async_reply({*tha, in_arp.tpa}, {in_arp.sha, in_arp.spa}, std::move(in_frame));
}

void arp::async_request(std::pair<mac_addr_t, ipv4_addr_t> const& from,
                        ipv4_addr_t const&                        to,
                        std::shared_ptr<device>                   dev) {
        arpv4_header_t const out_arp{
                .htype = 0x0001,
                .ptype = 0x0800,
                .hlen  = 0x06,
                .plen  = 0x04,
                .oper  = 0x0001,
                .sha   = from.first,
                .spa   = from.second,
                .tha   = {},
                .tpa   = to,
        };

        auto const room{ethernetv2_header_t::size() + arpv4_header_t::size()};

        auto skb_out = skbuff{
                std::make_unique_for_overwrite<std::byte[]>(room),
                room,
                ethernetv2_header_t::size(),
        };

        out_arp.produce_to_net(skb_out.head());

        spdlog::debug("[ARP] ENQUEUE REQUEST {}", out_arp);

        enqueue({
                .src_mac_addr = out_arp.sha,
                .dst_mac_addr =
                        std::array<std::byte, 6>{0xff_b, 0xff_b, 0xff_b, 0xff_b, 0xff_b, 0xff_b},
                .proto = PROTO,
                .skb   = std::move(skb_out),
                .dev   = std::move(dev),
        });
}

void arp::process_reply(arpv4_header_t const& in_arp) {
        spdlog::debug("[ARP] PROCESS REPLY");
        update({in_arp.sha, in_arp.spa});
        auto on_reply{std::exchange(on_replies_[in_arp.spa], {})};
        on_reply(in_arp.sha);
}

void arp::process(ethernetv2_frame&& in_frame) {
        assert(!(in_frame.skb.payload().size() < arpv4_header_t::size()));
        auto const in_arp{arpv4_header_t::consume_from_net(in_frame.skb.head())};
        in_frame.skb.pop_front(arpv4_header_t::size());

        spdlog::debug("[ARP] RECEIVE PACKET {}", in_arp);

        if (0x0001 == in_arp.htype && 0x0800 == in_arp.ptype) {
                switch (in_arp.oper) {
                        case 0x0001u:
                                process_request(in_arp, std::move(in_frame));
                                break;
                        case 0x0002u:
                                process_reply(in_arp);
                                break;
                        default:
                                spdlog::debug("[RECEIVED ARP] {}", in_arp);
                                break;
                }
        }
}

}  // namespace mstack
