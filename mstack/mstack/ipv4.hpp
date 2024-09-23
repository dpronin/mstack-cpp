#pragma once

#include <cstdint>

#include <memory>
#include <optional>

#include <boost/asio/io_context.hpp>

#include "arp.hpp"
#include "base_protocol.hpp"
#include "ethernetv2_frame.hpp"
#include "ipv4_packet.hpp"

namespace mstack {

class routing_table;

class ipv4 : public base_protocol<ethernetv2_frame, ipv4_packet> {
public:
        constexpr static uint16_t PROTO{0x0800};

        explicit ipv4(boost::asio::io_context&             io_ctx,
                      std::shared_ptr<routing_table const> rt,
                      arp&                                 arp);
        ~ipv4() = default;

        ipv4(ipv4 const&)            = delete;
        ipv4& operator=(ipv4 const&) = delete;

        ipv4(ipv4&&)            = delete;
        ipv4& operator=(ipv4&&) = delete;

private:
        void process(ipv4_packet&& in_packet) override;

        std::optional<ipv4_packet> make_packet(ethernetv2_frame&& in_packet) override;

        std::shared_ptr<routing_table const> rt_;
        arp&                                 arp_;

        uint16_t seq_{0};
};

}  // namespace mstack
