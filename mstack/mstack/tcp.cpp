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
                utils::hton(pkt_in.local_info.ipv4_addr.raw()) +
                        utils::hton(pkt_in.remote_info.ipv4_addr.raw()) +
                        utils::hton(static_cast<uint16_t>(pkt_in.proto)) +
                        utils::hton(static_cast<uint16_t>(pkt_in.skb.payload().size())),
        };

        auto tcph{tcp_header_t::consume_from_net(pkt_in.skb.head())};
        tcph.chsum = utils::checksum_net(pkt_in.skb.payload(), tcp_pseudo_header_chsum);
        tcph.produce_to_net(pkt_in.skb.head());

        enqueue({
                .src_ipv4_addr = pkt_in.local_info.ipv4_addr,
                .dst_ipv4_addr = pkt_in.remote_info.ipv4_addr,
                .proto         = pkt_in.proto,
                .skb           = std::move(pkt_in.skb),
        });
}

std::optional<tcp_packet> tcp::make_packet(ipv4_packet&& pkt_in) {
        auto const tcph{tcp_header_t::consume_from_net(pkt_in.skb.head())};

        spdlog::debug("[TCP] RECEIVE {}", tcph);

        auto tcp_pkt = tcp_packet{
                .proto = PROTO,
                .remote_info =
                        {
                                .ipv4_addr = pkt_in.src_ipv4_addr,
                                .port_addr = tcph.src_port,
                        },
                .local_info =
                        {
                                .ipv4_addr = pkt_in.dst_ipv4_addr,
                                .port_addr = tcph.dst_port,
                        },
                .skb = std::move(pkt_in.skb),
        };

        for (auto const& [matcher, proto, cb] : rules_) {
                if (matcher(tcp_pkt.remote_info, tcp_pkt.local_info)) {
                        spdlog::debug("[TCP] intercept {} -> {}", tcp_pkt.remote_info,
                                      tcp_pkt.local_info);
                        if (cb(std::move(tcp_pkt))) {
                                return std::nullopt;
                        }
                }
        }

        return tcp_pkt;
}

void tcp::rule_insert_front(
        std::function<bool(ipv4_port_t const& remote_info, ipv4_port_t const& local_info)> matcher,
        int                                                                                proto,
        std::function<bool(tcp_packet&& pkt_in)>                                           cb) {
        rules_.emplace_front(std::move(matcher), proto, std::move(cb));
}

void tcp::rule_insert_back(
        std::function<bool(ipv4_port_t const& remote_info, ipv4_port_t const& local_info)> matcher,
        int                                                                                proto,
        std::function<bool(tcp_packet&& pkt_in)>                                           cb) {
        rules_.emplace_back(std::move(matcher), proto, std::move(cb));
}

}  // namespace mstack
