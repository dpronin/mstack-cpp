#pragma once

#include <cstdint>

#include <functional>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/signals2/signal.hpp>

#include "arp_cache.hpp"
#include "arp_header.hpp"
#include "base_protocol.hpp"
#include "ethernetv2_frame.hpp"
#include "ipv4_addr.hpp"
#include "mac_addr.hpp"

namespace mstack {

class arp : public base_protocol<ethernetv2_frame, void> {
public:
        static constexpr uint16_t PROTO{0x0806};

        explicit arp(boost::asio::io_context& io_ctx, std::shared_ptr<arp_cache_t> arp_cache);
        ~arp() = default;

        arp(arp const&)            = delete;
        arp& operator=(arp const&) = delete;

        arp(arp&&)            = delete;
        arp& operator=(arp&&) = delete;

        void async_resolve(mac_addr_t const&                          from_mac,
                           ipv4_addr_t const&                         from_ipv4,
                           ipv4_addr_t const&                         to_ipv4,
                           std::shared_ptr<device>                    dev,
                           std::function<void(mac_addr_t const& mac)> cb);

private:
        void update(std::pair<mac_addr_t, ipv4_addr_t> const& peer);
        void async_reply(std::pair<mac_addr_t, ipv4_addr_t> const& from,
                         std::pair<mac_addr_t, ipv4_addr_t> const& to,
                         ethernetv2_frame&&                        in_frame);
        void process_request(arpv4_header_t const& in_arp, ethernetv2_frame&& in_frame);
        void async_request(std::pair<mac_addr_t, ipv4_addr_t> const& from,
                           ipv4_addr_t const&                        to,
                           std::shared_ptr<device>                   dev);
        void process_reply(arpv4_header_t const& in_arp);
        void process(ethernetv2_frame&& in_frame) override;

        std::shared_ptr<arp_cache_t> arp_cache_;
        std::unordered_map<ipv4_addr_t, boost::signals2::signal<void(mac_addr_t const& mac)>>
                on_replies_;
};

}  // namespace mstack
