#pragma once

#include <deque>
#include <functional>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include "base_protocol.hpp"
#include "endpoint.hpp"
#include "ipv4_packet.hpp"
#include "packets.hpp"
#include "tcp_packet.hpp"

namespace mstack {

class tcp : public base_protocol<ipv4_packet, tcp_packet> {
private:
        struct raw_state {
                std::queue<tcp_packet>                        pq;
                std::queue<std::function<void(tcp_packet&&)>> on_data_receive;
        };

        struct rule {
                std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> matcher;
                std::function<void(endpoint const& remote_ep, endpoint const& local_ep)> cb;
        };
        std::deque<rule> rules_;

        std::unordered_map<two_ends_t, raw_state> rcv_raw_states_;

public:
        constexpr static int PROTO{0x06};

        explicit tcp(boost::asio::io_context& io_ctx);
        ~tcp() = default;

        tcp(tcp const&)            = delete;
        tcp& operator=(tcp const&) = delete;

        tcp(tcp&&)            = delete;
        tcp& operator=(tcp&&) = delete;

        void process(tcp_packet&& pkt_in) override;

        void rule_insert_front(
                std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> matcher,
                std::function<void(endpoint const& remote_ep, endpoint const& local_ep)> cb);

        void rule_insert_back(
                std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> matcher,
                std::function<void(endpoint const& remote_ep, endpoint const& local_ep)> cb);

        void attach(endpoint const& remote_ep, endpoint const& local_ep);
        void detach(endpoint const& remote_ep, endpoint const& local_ep);

        void async_wait(endpoint const&                       remote_ep,
                        endpoint const&                       local_ep,
                        std::function<void(tcp_packet&& pkt)> cb);

        void async_write(::mstack::tcp_packet&&                                pkt,
                         std::function<void(boost::system::error_code const&)> cb);

private:
        std::optional<tcp_packet> make_packet(ipv4_packet&& pkt_in) override;

        void process_raw(tcp_packet&& pkt_in);
};

}  // namespace mstack
