#pragma once

#include <cassert>

#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <boost/asio/io_context.hpp>

#include <spdlog/spdlog.h>

#include "base_protocol.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcb.hpp"

namespace mstack {

class tcb_manager : public base_protocol<tcp_packet_t, void> {
private:
        std::shared_ptr<std::queue<std::shared_ptr<tcb_t>>>          active_tcbs_;
        std::unordered_map<two_ends_t, std::shared_ptr<tcb_t>>       tcbs_;
        std::unordered_set<ipv4_port_t>                              bound_;
        std::unordered_map<ipv4_port_t, std::shared_ptr<listener_t>> listeners_;

public:
        constexpr static int PROTO{0x06};

        explicit tcb_manager(boost::asio::io_context& io_ctx);
        ~tcb_manager() = default;

        tcb_manager(tcb_manager const&)            = delete;
        tcb_manager& operator=(tcb_manager const&) = delete;

        tcb_manager(tcb_manager&&)            = delete;
        tcb_manager& operator=(tcb_manager&&) = delete;

        void activate();

        std::shared_ptr<listener_t> listener_get(ipv4_port_t const& ipv4_port);
        void                        bind(ipv4_port_t const& ipv4_port);

        void listen(ipv4_port_t const& ipv4_port, std::shared_ptr<listener_t> listener);

        void process(tcp_packet_t&& in_pkt) override;
};

}  // namespace mstack
