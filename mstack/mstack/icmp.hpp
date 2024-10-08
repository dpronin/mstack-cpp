#include <cassert>
#include <cstdint>
#include <cstring>

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

        void process(ipv4_packet&& pkt_in) override {
                assert(!(pkt_in.skb.payload().size() < icmp_header_t::size()));

                auto const in_icmp_h{
                        icmp_header_t::consume_from_net(pkt_in.skb.head()),
                };

                pkt_in.skb.pop_front(icmp_header_t::size());

                spdlog::debug("[ICMP] RECEIVED {}", in_icmp_h);

                switch (in_icmp_h.proto_type) {
                        case 0x08:
                                process_request(in_icmp_h, std::move(pkt_in));
                                break;
                        default:
                                break;
                }
        }

private:
        void process_request(icmp_header_t const& in_icmp_h, ipv4_packet&& pkt_in) {
                async_reply(in_icmp_h, std::move(pkt_in));
        }

        void async_reply(icmp_header_t const& in_icmp_h, ipv4_packet&& pkt_in) {
                auto out_icmp_header = icmp_header_t{
                        .proto_type = 0x0,
                        .code       = 0x0,
                        .chsum      = 0x0,
                        .id         = in_icmp_h.id,
                        .seq        = in_icmp_h.seq,
                };

                auto skb_out{std::move(pkt_in.skb)};

                assert(!(skb_out.headroom() < icmp_header_t::size()));
                skb_out.push_front(icmp_header_t::size());

                out_icmp_header.produce_to_net(skb_out.head());

                uint16_t const chsum_net{utils::checksum(skb_out.payload())};
                std::memcpy(skb_out.head() + offsetof(icmp_header_t, chsum), &chsum_net,
                            sizeof(chsum_net));

                spdlog::debug("[ICMP] ENQUEUE REPLY {}", out_icmp_header);

                enqueue({
                        .src_addrv4 = pkt_in.dst_addrv4,
                        .dst_addrv4 = pkt_in.src_addrv4,
                        .proto      = pkt_in.proto,
                        .skb        = std::move(skb_out),
                });
        }
};

}  // namespace mstack
