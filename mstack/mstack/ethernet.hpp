#pragma once

#include <boost/asio/io_context.hpp>

#include "base_protocol.hpp"
#include "ethernetv2_frame.hpp"

namespace mstack {

class ethernetv2 : public base_protocol<void, ethernetv2_frame> {
public:
        explicit ethernetv2(boost::asio::io_context& io_ctx);
        ~ethernetv2() = default;

        ethernetv2(ethernetv2 const&)            = delete;
        ethernetv2& operator=(ethernetv2 const&) = delete;

        ethernetv2(ethernetv2&&)            = delete;
        ethernetv2& operator=(ethernetv2&&) = delete;

        void process(ethernetv2_frame&& in_packet) override;

private:
        std::optional<ethernetv2_frame> make_packet(skbuff&&                skb_in,
                                                    std::shared_ptr<device> dev) override;
};

}  // namespace mstack
