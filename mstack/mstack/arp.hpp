#pragma once

#include <cstdint>

#include "arp_cache.hpp"
#include "arp_header.hpp"
#include "base_protocol.hpp"
#include "defination.hpp"
#include "packets.hpp"

#include <spdlog/spdlog.h>

namespace mstack {

class arp : public base_protocol<ethernetv2_packet, ipv4_packet, arp> {
public:
        static constexpr uint16_t PROTO{0x0806};

        int id() override { return PROTO; }

        std::optional<mac_addr_t> query_mac(ipv4_addr_t const& ipv4_addr) {
                return arp_cache.query(ipv4_addr);
        }

        void remove(ipv4_addr_t const& k) { arp_cache.reset(k); }

        void add(std::pair<ipv4_addr_t, mac_addr_t> const& kv) { arp_cache.update(kv); }

        void send_reply(arpv4_header_t const& in_arp) {
                auto const& dev_mac_addr{arp_cache.query(in_arp.dst_ipv4_addr)};

                struct arpv4_header_t out_arp;
                out_arp.hw_type    = 0x0001;
                out_arp.proto_type = 0x0800;
                out_arp.hw_size    = 0x06;
                out_arp.proto_size = 0x04;
                out_arp.opcode     = 0x02;

                out_arp.src_mac_addr  = dev_mac_addr.value();
                out_arp.src_ipv4_addr = in_arp.dst_ipv4_addr;
                out_arp.dst_mac_addr  = in_arp.src_mac_addr;
                out_arp.dst_ipv4_addr = in_arp.src_ipv4_addr;

                arp_cache.update(in_arp.src_ipv4_addr, in_arp.src_mac_addr);

                auto out_buffer{std::make_unique<base_packet>(arpv4_header_t::size())};
                out_arp.produce(out_buffer->get_pointer());

                ethernetv2_packet out_packet = {
                        .src_mac_addr = out_arp.src_mac_addr,
                        .dst_mac_addr = out_arp.dst_mac_addr,
                        .proto        = PROTO,
                        .buffer       = std::move(out_buffer),
                };

                this->enter_send_queue(std::move(out_packet));

                spdlog::debug("[ARP] SEND ARP REPLY {}", out_arp);
        };

        std::optional<ipv4_packet> make_packet(ethernetv2_packet&& in_packet) override {
                auto const in_arp{
                        arpv4_header_t::consume(in_packet.buffer->get_pointer()),
                };

                if (in_arp.opcode == 0x0001) send_reply(in_arp);

                return std::nullopt;
        }

private:
        arp_cache_t arp_cache;
};

}  // namespace mstack
