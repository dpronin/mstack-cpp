#pragma once

#include <functional>

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include "base_protocol.hpp"
#include "ipv4_packet.hpp"
#include "tcp_packet.hpp"

namespace mstack {

class tcp : public base_protocol<ipv4_packet, tcp_packet> {
public:
        constexpr static int PROTO{0x06};

        explicit tcp(boost::asio::io_context& io_ctx);
        ~tcp() = default;

        tcp(tcp const&)            = delete;
        tcp& operator=(tcp const&) = delete;

        tcp(tcp&&)            = delete;
        tcp& operator=(tcp&&) = delete;

        void process(tcp_packet&& pkt_in) override;

        void rule_insert_front(std::function<bool(ipv4_port_t const& remote_info,
                                                  ipv4_port_t const& local_info)> matcher,
                               int                                                proto,
                               std::function<bool(tcp_packet const& pkt_in)>      cb);

        void rule_insert_back(std::function<bool(ipv4_port_t const& remote_info,
                                                 ipv4_port_t const& local_info)> matcher,
                              int                                                proto,
                              std::function<bool(tcp_packet const& pkt_in)>      cb);

private:
        std::optional<tcp_packet> make_packet(ipv4_packet&& pkt_in) override;

        struct rule {
                std::function<bool(ipv4_port_t const& remote_info, ipv4_port_t const& local_info)>
                                                              matcher;
                int                                           proto;
                std::function<bool(tcp_packet const& pkt_in)> cb;
        };
        std::deque<rule> rules_;
};

}  // namespace mstack
