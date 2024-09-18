#include <cstdint>

#include <spdlog/spdlog.h>

#include "base_protocol.hpp"
#include "icmp_header.hpp"
#include "ipv4_packet.hpp"

namespace mstack {

class icmp : public base_protocol<ipv4_packet, void, icmp> {
public:
        static constexpr uint16_t PROTO{0x01};

private:
        void make_icmp_reply(ipv4_packet const& in_packet) {
                auto const in_icmp_header{
                        icmp_header_t::consume(in_packet.buffer->get_pointer()),
                };

                auto out_icmp_header{
                        icmp_header_t{
                                .id  = in_icmp_header.id,
                                .seq = in_icmp_header.seq,
                        },
                };

                auto out_buffer{
                        std::make_unique<base_packet>(in_packet.buffer->get_remaining_len()),
                };

                std::byte* payload_pointer{
                        out_icmp_header.produce(out_buffer->get_pointer()),
                };

                in_packet.buffer->export_payload(payload_pointer, icmp_header_t::size());

                std::byte* pointer{out_buffer->get_pointer()};
                auto const checksum{
                        utils::checksum(
                                {pointer, static_cast<size_t>(out_buffer->get_remaining_len())}, 0),
                };

                out_icmp_header.checksum = checksum;
                out_icmp_header.produce(pointer);

                spdlog::debug("{}", out_icmp_header);

                auto out_packet{
                        ipv4_packet{
                                .src_ipv4_addr = in_packet.dst_ipv4_addr,
                                .dst_ipv4_addr = in_packet.src_ipv4_addr,
                                .proto         = in_packet.proto,
                                .buffer        = std::move(out_buffer),
                        },
                };

                spdlog::debug("[ENQUEUE ICMP REPLY]");

                this->enqueue(std::move(out_packet));
        }

        void process(ipv4_packet&& in_packet) {
                auto const in_icmp_header{icmp_header_t::consume(in_packet.buffer->get_pointer())};

                spdlog::debug("[RECEIVED ICMP] {}", in_icmp_header);

                if (in_icmp_header.proto_type == 0x08) make_icmp_reply(in_packet);
        }
};

}  // namespace mstack
