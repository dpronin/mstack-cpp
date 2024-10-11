#pragma once

#include <cassert>

#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <spdlog/spdlog.h>

#include "base_protocol.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcb.hpp"

namespace mstack {

class tcb_manager : public base_protocol<tcp_packet, void> {
private:
        class port_generator_ctx;
        std::unique_ptr<port_generator_ctx> port_gen_ctx_;

        struct rule {
                std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> matcher;
                std::function<void(boost::system::error_code const& ec,
                                   endpoint const&                  remote_ep,
                                   endpoint const&                  local_ep,
                                   std::weak_ptr<tcb_t>             tcb)>
                        cb;
        };
        std::deque<rule> rules_;

        std::unordered_map<two_ends_t, std::shared_ptr<tcb_t>> tcbs_;

public:
        constexpr static int PROTO{0x06};

        using base_protocol::enqueue;

        explicit tcb_manager(boost::asio::io_context& io_ctx);
        ~tcb_manager() noexcept;

        tcb_manager(tcb_manager const&)            = delete;
        tcb_manager& operator=(tcb_manager const&) = delete;

        tcb_manager(tcb_manager&&)            = delete;
        tcb_manager& operator=(tcb_manager&&) = delete;

        void async_connect(endpoint const&                               remote_ep,
                           ipv4_addr_t const&                            local_addr,
                           std::function<void(boost::system::error_code const& ec,
                                              endpoint const&                  remote_ep,
                                              endpoint const&                  local_ep,
                                              std::weak_ptr<tcb_t>             tcb)> cb);
        void async_connect(endpoint const&                           remote_ep,
                           endpoint const&                           local_ep,
                           std::function<void(boost::system::error_code const& ec,
                                              endpoint const&                  remote_ep,
                                              endpoint const&                  local_ep,
                                              std::weak_ptr<tcb_t>)> cb);

        void rule_insert_front(
                std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> matcher,
                std::function<void(boost::system::error_code const& ec,
                                   endpoint const&                  remote_ep,
                                   endpoint const&                  local_ep,
                                   std::weak_ptr<tcb_t>)>                                cb);

        void rule_insert_back(
                std::function<bool(endpoint const& remote_ep, endpoint const& local_ep)> matcher,
                std::function<void(boost::system::error_code const& ec,
                                   endpoint const&                  remote_ep,
                                   endpoint const&                  local_ep,
                                   std::weak_ptr<tcb_t>)>                                cb);

        void process(tcp_packet&& pkt_in) override;
};

}  // namespace mstack
