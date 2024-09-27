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
                        icmp_header_t::consume_from_net(in_pkt.buffer->get_pointer()),
                };

                spdlog::debug("[RECEIVED ICMP] {}", in_icmp_h);

                switch (in_icmp_h.proto_type) {
                        case 0x08:
                                process_request(in_icmp_h, in_pkt);
                                break;
                        default:
                                break;
                }
        }

private:
        void process_request(icmp_header_t const& in_icmp_h, ipv4_packet const& in_pkt) {
                async_reply(in_icmp_h, in_pkt);
        }

        void async_reply(icmp_header_t const& in_icmp_h, ipv4_packet const& in_packet) {
                auto out_icmp_header{
                        icmp_header_t{
                                .id  = in_icmp_h.id,
                                .seq = in_icmp_h.seq,
                        },
                };

                auto out_buffer{
                        std::make_unique<base_packet>(in_packet.buffer->get_remaining_len()),
                };

                std::byte* payload_pointer{
                        out_icmp_header.produce_to_net(out_buffer->get_pointer()),
                };

                in_packet.buffer->export_payload(payload_pointer, icmp_header_t::size());

                std::byte* pointer{out_buffer->get_pointer()};
                auto const checksum{
                        utils::checksum_net(
                                {pointer, static_cast<size_t>(out_buffer->get_remaining_len())}),
                };

                out_icmp_header.checksum = checksum;
                out_icmp_header.produce_to_net(pointer);

                spdlog::debug("[ICMP] ENQUEUE REPLY {}", out_icmp_header);

                enqueue({
                        .src_ipv4_addr = in_packet.dst_ipv4_addr,
                        .dst_ipv4_addr = in_packet.src_ipv4_addr,
                        .proto         = in_packet.proto,
                        .buffer        = std::move(out_buffer),
                });
        }
};

}  // namespace mstack
