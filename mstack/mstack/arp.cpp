#include "arp.hpp"

#include <cassert>

#include <utility>

#include <spdlog/spdlog.h>

namespace mstack {

arp::arp(boost::asio::io_context& io_ctx, std::shared_ptr<arp_cache_t> arp_cache)
    : base_protocol(io_ctx), arp_cache_(std::move(arp_cache)) {
        assert(arp_cache_);
}

void arp::async_resolve(mac_addr_t const&                          from_mac,
                        ipv4_addr_t const&                         from_ipv4,
                        ipv4_addr_t const&                         to_ipv4,
                        std::function<void(mac_addr_t const& mac)> cb) {
        if (auto const mac{arp_cache_->query(to_ipv4)}) {
                io_ctx_.post([cb = std::move(cb), mac = *mac] mutable { cb(mac); });
        } else {
                on_replies_[to_ipv4].connect(cb);
                async_request({from_mac, from_ipv4}, to_ipv4);
        }
}

void arp::update(std::pair<mac_addr_t, ipv4_addr_t> const& peer) {
        if (!(peer.first.is_broadcast() || peer.first.is_zero())) {
                spdlog::debug("[ARP] update cache {} -> {}", peer.first, peer.second);
                arp_cache_->update({peer.second, peer.first});
        }
}

void arp::async_reply(std::pair<mac_addr_t, ipv4_addr_t> const& from,
                      std::pair<mac_addr_t, ipv4_addr_t> const& to) {
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

        auto out_buffer{std::make_unique<base_packet>(arpv4_header_t::size())};
        out_arp.produce(out_buffer->get_pointer());

        ethernetv2_frame out_packet{
                .src_mac_addr = out_arp.sha,
                .dst_mac_addr = out_arp.tha,
                .proto        = PROTO,
                .buffer       = std::move(out_buffer),
        };

        spdlog::debug("[ARP] ENQUEUE ARP REPLY {}", out_arp);

        enqueue(std::move(out_packet));
}

void arp::process_request(arpv4_header_t const& in_arp) {
        spdlog::debug("[ARP] PROCESS REQ");
        update({in_arp.sha, in_arp.spa});
        if (auto const& tha{arp_cache_->query(in_arp.tpa)})
                async_reply({*tha, in_arp.tpa}, {in_arp.sha, in_arp.spa});
}

void arp::async_request(std::pair<mac_addr_t, ipv4_addr_t> const& from, ipv4_addr_t const& to) {
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

        auto out_buffer{std::make_unique<base_packet>(arpv4_header_t::size())};
        out_arp.produce(out_buffer->get_pointer());

        ethernetv2_frame out_packet = {
                .src_mac_addr = out_arp.sha,
                .dst_mac_addr = out_arp.tha,
                .proto        = PROTO,
                .buffer       = std::move(out_buffer),
        };

        spdlog::debug("[ARP] ENQUEUE ARP REQUEST {}", out_arp);

        enqueue(std::move(out_packet));
}

void arp::process_reply(arpv4_header_t const& in_arp) {
        spdlog::debug("[ARP] PROCESS REPLY");
        update({in_arp.sha, in_arp.spa});
        auto on_reply{std::exchange(on_replies_[in_arp.spa], {})};
        on_reply(in_arp.sha);
}

void arp::process(ethernetv2_frame&& in_packet) {
        auto const in_arp{
                arpv4_header_t::consume(in_packet.buffer->get_pointer()),
        };

        spdlog::debug("[ARP] RECEIVE PACKET {}", in_arp);

        if (0x0001 == in_arp.htype && 0x0800 == in_arp.ptype) {
                switch (in_arp.oper) {
                        case 0x0001u:
                                process_request(in_arp);
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
