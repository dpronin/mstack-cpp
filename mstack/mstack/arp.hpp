#pragma once

#include <cassert>
#include <cstdint>

#include "arp_cache.hpp"
#include "arp_header.hpp"
#include "base_protocol.hpp"
#include "ethernetv2_frame.hpp"
#include "mac_addr.hpp"

#include <spdlog/spdlog.h>

namespace mstack {

class arp : public base_protocol<ethernetv2_frame, void, arp> {
public:
        static constexpr uint16_t PROTO{0x0806};

        explicit arp(std::shared_ptr<arp_cache_t> arp_cache) : arp_cache_(std::move(arp_cache)) {
                assert(arp_cache_);
        }
        ~arp() = default;

        arp(arp const&)            = delete;
        arp& operator=(arp const&) = delete;

        arp(arp&&)            = delete;
        arp& operator=(arp&&) = delete;

        void remove(ipv4_addr_t const& k) { arp_cache_->reset(k); }

        void add(std::pair<ipv4_addr_t, mac_addr_t> const& kv) { arp_cache_->update(kv); }

private:
        void send_reply(arpv4_header_t const& in_arp, mac_addr_t const& sha) {
                arpv4_header_t const out_arp = {
                        .htype = 0x0001,
                        .ptype = 0x0800,
                        .hlen  = 0x06,
                        .plen  = 0x04,
                        .oper  = 0x02,
                        .sha   = sha,
                        .spa   = in_arp.tpa,
                        .tha   = in_arp.sha,
                        .tpa   = in_arp.spa,
                };

                arp_cache_->update(in_arp.spa, in_arp.sha);

                auto out_buffer{std::make_unique<base_packet>(arpv4_header_t::size())};
                out_arp.produce(out_buffer->get_pointer());

                ethernetv2_frame out_packet = {
                        .src_mac_addr = out_arp.sha,
                        .dst_mac_addr = out_arp.tha,
                        .proto        = PROTO,
                        .buffer       = std::move(out_buffer),
                };

                spdlog::debug("[ARP] ENQUEUE ARP REPLY {}", out_arp);

                this->enqueue(std::move(out_packet));
        }

        void send_request(mac_addr_t const& sha, ipv4_addr_t const& spa, ipv4_addr_t const& tpa) {
                arpv4_header_t const out_arp = {
                        .htype = 0x0001,
                        .ptype = 0x0800,
                        .hlen  = 0x06,
                        .plen  = 0x04,
                        .oper  = 0x01,
                        .sha   = sha,
                        .spa   = spa,
                        .tha   = std::array<uint8_t, 6>{0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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

        void process(ethernetv2_frame&& in_packet) override {
                auto const in_arp{
                        arpv4_header_t::consume(in_packet.buffer->get_pointer()),
                };

                spdlog::debug("[RECEIVED ARP] {}", in_arp);

                if (in_arp.oper == 0x01) {
                        if (auto const& dev_mac_addr{arp_cache_->query(in_arp.tpa)}) {
                                send_reply(in_arp, *dev_mac_addr);
                        }
                }
        }

private:
        std::shared_ptr<arp_cache_t> arp_cache_;
};

}  // namespace mstack
