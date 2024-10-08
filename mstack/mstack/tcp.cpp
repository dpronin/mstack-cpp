#include "tcp.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

#include <spdlog/spdlog.h>

#include "mstack/tcp_packet.hpp"
#include "packets.hpp"
#include "tcp_header.hpp"
#include "utils.hpp"

namespace mstack {

tcp::tcp(boost::asio::io_context& io_ctx) : base_protocol(io_ctx) {}

void tcp::process_raw(tcp_packet&& pkt_in) {
        enqueue({
                .src_addrv4 = pkt_in.local_ep.addrv4,
                .dst_addrv4 = pkt_in.remote_ep.addrv4,
                .proto      = pkt_in.proto,
                .skb        = std::move(pkt_in.skb),
        });
}

void tcp::process(tcp_packet&& pkt_in) {
        spdlog::debug("[TCP] HDL FROM U-LAYER {}", pkt_in);

        assert(!(pkt_in.skb.payload().size() < tcp_header_t::fixed_size()));

        std::memset(pkt_in.skb.head() + offsetof(tcp_header_t, chsum), 0x00,
                    sizeof(tcp_header_t::chsum));

        struct tcp_pseudo_header {
                uint32_t src_addrv4;
                uint32_t dst_addrv4;
                uint8_t  reserved;
                uint8_t  proto;
                uint16_t segment_len;
        } const tcp_ph_net = {
                .src_addrv4  = utils::hton(pkt_in.local_ep.addrv4.raw()),
                .dst_addrv4  = utils::hton(pkt_in.remote_ep.addrv4.raw()),
                .reserved    = 0,
                .proto       = pkt_in.proto,
                .segment_len = utils::hton(static_cast<uint16_t>(pkt_in.skb.payload().size())),
        };

        assert(!(pkt_in.skb.headroom() < sizeof(tcp_ph_net)));
        pkt_in.skb.push_front(sizeof(tcp_ph_net));
        std::memcpy(pkt_in.skb.head(), &tcp_ph_net, sizeof(tcp_ph_net));

        decltype(tcp_header_t::chsum) const chsum_net{utils::checksum(pkt_in.skb.payload())};
        pkt_in.skb.pop_front(sizeof(tcp_ph_net));
        std::memcpy(pkt_in.skb.head() + offsetof(tcp_header_t, chsum), &chsum_net,
                    sizeof(chsum_net));

        process_raw(std::move(pkt_in));
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

        auto const two_end = two_ends_t{
                .remote_ep = tcp_pkt.remote_ep,
                .local_ep  = tcp_pkt.local_ep,
        };

        if (auto it{rcv_raw_states_.find(two_end)}; rcv_raw_states_.end() != it) {
                if (!it->second.on_data_receive.empty()) {
                        auto cb{std::move(it->second.on_data_receive.front())};
                        it->second.on_data_receive.pop();
                        cb(std::move(tcp_pkt));
                } else {
                        it->second.pq.push(std::move(tcp_pkt));
                }
                return std::nullopt;
        } else {
                for (auto const& [matcher, cb] : rules_) {
                        if (matcher(two_end.remote_ep, two_end.local_ep)) {
                                spdlog::debug("[TCP] new interception {} -> {}", two_end.remote_ep,
                                              two_end.local_ep);
                                it = rcv_raw_states_.emplace_hint(it, two_end, raw_state{});
                                try {
                                        it->second.pq.push(std::move(tcp_pkt));
                                        cb(two_end.remote_ep, two_end.local_ep);
                                        return std::nullopt;
                                } catch (std::exception const& ex) {
                                        spdlog::warn(
                                                "[TCP] failed to intercept {} -> {}, reason: {}",
                                                two_end.remote_ep, two_end.local_ep, ex.what());
                                        rcv_raw_states_.erase(it);
                                }
                        }
                }
        }

        return tcp_pkt;
}

void tcp::rule_insert_front(
        std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> matcher,
        std::function<void(endpoint const& remote_ep, endpoint const& local_ep)> cb) {
        rules_.emplace_front(std::move(matcher), std::move(cb));
}

void tcp::rule_insert_back(
        std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> matcher,
        std::function<void(endpoint const& remote_ep, endpoint const& local_ep)> cb) {
        rules_.emplace_back(std::move(matcher), std::move(cb));
}

void tcp::attach(endpoint const& remote_ep, endpoint const& local_ep) {
        auto const key      = two_ends_t{.remote_ep = remote_ep, .local_ep = local_ep};
        auto [it, emplaced] = rcv_raw_states_.emplace(key, raw_state{});
        if (!emplaced) throw std::runtime_error{fmt::format("{} is already attached", key)};
}

void tcp::detach(endpoint const& remote_ep, endpoint const& local_ep) {
        rcv_raw_states_.erase({.remote_ep = remote_ep, .local_ep = local_ep});
}

void tcp::async_wait(endpoint const&                   remote_ep,
                     endpoint const&                   local_ep,
                     std::function<void(tcp_packet&&)> cb) {
        auto const two_end = two_ends_t{
                .remote_ep = remote_ep,
                .local_ep  = local_ep,
        };
        if (auto it{rcv_raw_states_.find(two_end)}; rcv_raw_states_.end() != it) {
                if (!it->second.pq.empty()) {
                        auto tcp_pkt{std::move(it->second.pq.front())};
                        it->second.pq.pop();
                        io_ctx_.post([tcp_pkt_w = std::make_shared<tcp_packet>(std::move(tcp_pkt)),
                                      cb = std::move(cb)]() mutable { cb(std::move(*tcp_pkt_w)); });
                } else {
                        it->second.on_data_receive.push(std::move(cb));
                }
        } else {
                throw std::runtime_error{"endpoint is not connected"};
        }
}

void tcp::async_write(::mstack::tcp_packet&&                                pkt,
                      std::function<void(boost::system::error_code const&)> cb) {
        process(std::move(pkt));
        io_ctx_.post([cb = std::move(cb)] { cb({}); });
}

}  // namespace mstack
