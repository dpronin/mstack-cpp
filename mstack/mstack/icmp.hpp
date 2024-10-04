#include <cstdint>

#include <spdlog/spdlog.h>

#include "base_protocol.hpp"
#include "icmp_header.hpp"
#include "ipv4_packet.hpp"

namespace mstack {

class icmp : public base_protocol<ipv4_packet, void> {
public:
        static constexpr uint16_t PROTO{0x01};

        explicit icmp(boost::asio::io_context& io_ctx) : base_protocol(io_ctx) {}
        ~icmp() = default;

        icmp(icmp const&)            = delete;
        icmp& operator=(icmp const&) = delete;

        icmp(icmp&&)            = delete;
        icmp& operator=(icmp&&) = delete;

        void process(ipv4_packet&& in_pkt) override {
                auto const in_icmp_h{
                        icmp_header_t::consume_from_net(in_pkt.skb.head()),
                };
                in_pkt.skb.pop_front(icmp_header_t::size());

                spdlog::debug("[RECEIVED ICMP] {}", in_icmp_h);

                switch (in_icmp_h.proto_type) {
                        case 0x08:
                                process_request(in_icmp_h, std::move(in_pkt));
                                break;
                        default:
                                break;
                }
        }

private:
        void process_request(icmp_header_t const& in_icmp_h, ipv4_packet&& in_pkt) {
                async_reply(in_icmp_h, std::move(in_pkt));
        }

        void async_reply(icmp_header_t const& in_icmp_h, ipv4_packet&& in_pkt) {
                auto out_icmp_header{
                        icmp_header_t{
                                .id  = in_icmp_h.id,
                                .seq = in_icmp_h.seq,
                        },
                };

                auto skb_out{std::move(in_pkt.skb)};

                skb_out.push_front(icmp_header_t::size());
                out_icmp_header.produce_to_net(skb_out.head());

                out_icmp_header.checksum = utils::checksum_net(skb_out.payload());
                out_icmp_header.produce_to_net(skb_out.head());

                spdlog::debug("[ICMP] ENQUEUE REPLY {}", out_icmp_header);

                enqueue({
                        .src_ipv4_addr = in_pkt.dst_ipv4_addr,
                        .dst_ipv4_addr = in_pkt.src_ipv4_addr,
                        .proto         = in_pkt.proto,
                        .skb           = std::move(skb_out),
                });
        }
};

}  // namespace mstack
