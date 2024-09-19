#pragma once

#include <cassert>
#include <cstdint>

#include <utility>

#include "arp_cache.hpp"
#include "arp_header.hpp"
#include "base_protocol.hpp"
#include "ethernetv2_frame.hpp"
#include "ipv4_addr.hpp"
#include "mac_addr.hpp"
#include "routing_table.hpp"

#include <spdlog/spdlog.h>

namespace mstack {

class arp : public base_protocol<ethernetv2_frame, void, arp> {
public:
        static constexpr uint16_t PROTO{0x0806};

        explicit arp(std::shared_ptr<routing_table> rt, std::shared_ptr<arp_cache_t> arp_cache)
            : rt_(std::move(rt)), arp_cache_(std::move(arp_cache)) {
                assert(rt_);
                assert(arp_cache_);
        }
        ~arp() = default;

        arp(arp const&)            = delete;
        arp& operator=(arp const&) = delete;

        arp(arp&&)            = delete;
        arp& operator=(arp&&) = delete;

private:
        void update(std::pair<mac_addr_t, ipv4_addr_t> const& peer) {
                if (!(peer.first.is_broadcast() || peer.first.is_zero())) {
                        spdlog::debug("[ARP] update cache {} -> {}", peer.first, peer.second);
                        rt_->update({peer.second, peer.second});
                        arp_cache_->update({peer.second, peer.first});
                }
        }

        void send_reply(std::pair<mac_addr_t, ipv4_addr_t> const& from,
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

                this->enqueue(std::move(out_packet));
        }

        void process_request(arpv4_header_t const& in_arp) {
                spdlog::debug("[ARP] PROCESS REQ");
                update({in_arp.sha, in_arp.spa});
                if (auto const& tha{arp_cache_->query(in_arp.tpa)})
                        send_reply({*tha, in_arp.tpa}, {in_arp.sha, in_arp.spa});
        }

        void send_request(std::pair<mac_addr_t, ipv4_addr_t> const& from, ipv4_addr_t const& tpa) {
                arpv4_header_t const out_arp{
                        .htype = 0x0001,
                        .ptype = 0x0800,
                        .hlen  = 0x06,
                        .plen  = 0x04,
                        .oper  = 0x0001,
                        .sha   = from.first,
                        .spa   = from.second,
                        .tha   = {},
                        .tpa   = tpa,
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

                this->enqueue(std::move(out_packet));
        }

        void process_reply(arpv4_header_t const& in_arp) {
                spdlog::debug("[ARP] PROCESS REPLY");
                update({in_arp.sha, in_arp.spa});
        }

        void process(ethernetv2_frame&& in_packet) override {
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

        std::shared_ptr<routing_table> rt_;
        std::shared_ptr<arp_cache_t>   arp_cache_;
};

}  // namespace mstack
