#pragma once

#include <cassert>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <boost/asio/io_context.hpp>

#include <spdlog/spdlog.h>

#include "base_protocol.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcb.hpp"

namespace mstack {

class tcb_manager : public base_protocol<tcp_packet, void> {
private:
        struct port_generator_ctx;
        std::unique_ptr<port_generator_ctx> port_gen_ctx_;

        std::unordered_map<two_ends_t, std::shared_ptr<tcb_t>>       tcbs_;
        std::unordered_set<ipv4_port_t>                              bound_;
        std::unordered_map<ipv4_port_t, std::shared_ptr<listener_t>> listeners_;

public:
        constexpr static int PROTO{0x06};

        using base_protocol::enqueue;

        explicit tcb_manager(boost::asio::io_context& io_ctx);
        ~tcb_manager() noexcept;

        tcb_manager(tcb_manager const&)            = delete;
        tcb_manager& operator=(tcb_manager const&) = delete;

        tcb_manager(tcb_manager&&)            = delete;
        tcb_manager& operator=(tcb_manager&&) = delete;

        void async_connect(endpoint const&                           ep,
                           std::function<void(boost::system::error_code const& ec,
                                              ipv4_port_t const&,
                                              std::weak_ptr<tcb_t>)> cb);

        std::shared_ptr<listener_t> listener_get(ipv4_port_t const& ipv4_port);
        void                        bind(ipv4_port_t const& ipv4_port);

        void listen(ipv4_port_t const& ipv4_port, std::shared_ptr<listener_t> listener);

        void process(tcp_packet&& in_pkt) override;
};

}  // namespace mstack
