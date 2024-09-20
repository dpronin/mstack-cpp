#pragma once

#include <cassert>

#include <memory>
#include <optional>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include <boost/asio/io_context.hpp>

#include <spdlog/spdlog.h>

#include "base_protocol.hpp"
#include "circle_buffer.hpp"
#include "defination.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcb.hpp"
#include "tcp_transmit.hpp"

namespace mstack {

class tcb_manager : public base_protocol<tcp_packet_t, void> {
private:
        std::shared_ptr<circle_buffer<std::shared_ptr<tcb_t>>>       active_tcbs_;
        std::unordered_map<two_ends_t, std::shared_ptr<tcb_t>>       tcbs_;
        std::unordered_set<ipv4_port_t>                              bound_;
        std::unordered_map<ipv4_port_t, std::shared_ptr<listener_t>> listeners_;

public:
        constexpr static int PROTO{0x06};

        explicit tcb_manager(boost::asio::io_context& io_ctx)
            : base_protocol(io_ctx),
              active_tcbs_(std::make_shared<circle_buffer<std::shared_ptr<tcb_t>>>()) {}
        ~tcb_manager() = default;

        tcb_manager(tcb_manager const&)            = delete;
        tcb_manager& operator=(tcb_manager const&) = delete;

        tcb_manager(tcb_manager&&)            = delete;
        tcb_manager& operator=(tcb_manager&&) = delete;

        void process_finish() {
                while (!active_tcbs_->empty()) {
                        if (auto tcb{active_tcbs_->pop()}) {
                                if (auto tcp_pkt{tcb.value()->gather_packet()}) {
                                        enqueue(std::move(*tcp_pkt));
                                }
                        }
                }
        }

        std::shared_ptr<listener_t> listener_get(ipv4_port_t const& ipv4_port) {
                return listeners_[ipv4_port];
        }

        void bind(ipv4_port_t const& ipv4_port) {
                if (!bound_.insert(ipv4_port).second)
                        throw std::system_error(std::make_error_code(std::errc::address_in_use));
        }

        void listen(ipv4_port_t const& ipv4_port, std::shared_ptr<listener_t> listener) {
                if (!bound_.contains(ipv4_port)) {
                        throw std::system_error(
                                std::make_error_code(std::errc::address_not_available));
                }
                assert(listener);
                listeners_[ipv4_port] = std::move(listener);
        }

        void process(tcp_packet_t&& in_pkt) override {
                two_ends_t const two_end = {
                        .remote_info = in_pkt.remote_info,
                        .local_info  = in_pkt.local_info,
                };

                if (auto tcb_it{tcbs_.find(two_end)}; tcbs_.end() != tcb_it) {
                        tcp_transmit::tcp_in(tcb_it->second, std::move(in_pkt));
                } else if (bound_.contains(in_pkt.local_info)) {
                        spdlog::debug("[TCB MNGR] reg {}", two_end);
                        tcb_it = tcbs_.emplace_hint(
                                tcb_it, two_end,
                                std::make_shared<tcb_t>(active_tcbs_, listeners_[in_pkt.local_info],
                                                        two_end.remote_info, two_end.local_info));
                        tcb_it->second->state      = TCP_LISTEN;
                        tcb_it->second->next_state = TCP_LISTEN;
                        tcp_transmit::tcp_in(tcb_it->second, std::move(in_pkt));
                } else {
                        spdlog::warn("[TCB MNGR] receive unknown TCP packet");
                }

                process_finish();
        }
};

}  // namespace mstack
