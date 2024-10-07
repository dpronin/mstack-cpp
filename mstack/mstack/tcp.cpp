#include "tcp.hpp"

#include <cstdint>

#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

#include "tcp_header.hpp"
#include "utils.hpp"

namespace mstack {

tcp::tcp(boost::asio::io_context& io_ctx) : base_protocol(io_ctx) {}

void tcp::process(tcp_packet&& pkt_in) {
        spdlog::debug("[TCP] HDL FROM U-LAYER {}", pkt_in);

        uint32_t const tcp_pseudo_header_chsum{
                utils::hton(pkt_in.local_ep.addrv4.raw()) +
                        utils::hton(pkt_in.remote_ep.addrv4.raw()) +
                        utils::hton(static_cast<uint16_t>(pkt_in.proto)) +
                        utils::hton(static_cast<uint16_t>(pkt_in.skb.payload().size())),
        };

        auto tcph{tcp_header_t::consume_from_net(pkt_in.skb.head())};
        tcph.chsum = utils::checksum_net(pkt_in.skb.payload(), tcp_pseudo_header_chsum);
        tcph.produce_to_net(pkt_in.skb.head());

        enqueue({
                .src_addrv4 = pkt_in.local_ep.addrv4,
                .dst_addrv4 = pkt_in.remote_ep.addrv4,
                .proto      = pkt_in.proto,
                .skb        = std::move(pkt_in.skb),
        });
}

std::optional<tcp_packet> tcp::make_packet(ipv4_packet&& pkt_in) {
        auto const tcph{tcp_header_t::consume_from_net(pkt_in.skb.head())};

        spdlog::debug("[TCP] RECEIVE {}", tcph);

        auto tcp_pkt = tcp_packet{
                .proto = PROTO,
                .remote_ep =
                        {
                                .addrv4      = pkt_in.src_addrv4,
                                .addrv4_port = tcph.src_port,
                        },
                .local_ep =
                        {
                                .addrv4      = pkt_in.dst_addrv4,
                                .addrv4_port = tcph.dst_port,
                        },
                .skb = std::move(pkt_in.skb),
        };

        for (auto const& [matcher, proto, cb] : rules_) {
                if (matcher(tcp_pkt.remote_ep, tcp_pkt.local_ep)) {
                        spdlog::debug("[TCP] intercept {} -> {}", tcp_pkt.remote_ep,
                                      tcp_pkt.local_ep);
                        if (cb(tcp_pkt)) {
                                return std::nullopt;
                        }
                }
        }

        return tcp_pkt;
}

void tcp::rule_insert_front(
        std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> matcher,
        int                                                                      proto,
        std::function<bool(tcp_packet const& pkt_in)>                            cb) {
        rules_.emplace_front(std::move(matcher), proto, std::move(cb));
}

void tcp::rule_insert_back(
        std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> matcher,
        int                                                                      proto,
        std::function<bool(tcp_packet const& pkt_in)>                            cb) {
        rules_.emplace_back(std::move(matcher), proto, std::move(cb));
}

}  // namespace mstack
