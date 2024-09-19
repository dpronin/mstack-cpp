#pragma once

#include <cassert>

#include <memory>
#include <optional>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include "circle_buffer.hpp"
#include "defination.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcb.hpp"
#include "tcp_transmit.hpp"

namespace mstack {

class tcb_manager {
private:
        std::shared_ptr<circle_buffer<std::shared_ptr<tcb_t>>>       active_tcbs_;
        std::unordered_map<two_ends_t, std::shared_ptr<tcb_t>>       tcbs_;
        std::unordered_set<ipv4_port_t>                              bound_;
        std::unordered_map<ipv4_port_t, std::shared_ptr<listener_t>> listeners_;

public:
        constexpr static int PROTO{0x06};

        tcb_manager() : active_tcbs_(std::make_shared<circle_buffer<std::shared_ptr<tcb_t>>>()) {}
        ~tcb_manager() = default;

        tcb_manager(const tcb_manager&) = delete;
        tcb_manager(tcb_manager&&)      = delete;

        tcb_manager& operator=(const tcb_manager&) = delete;
        tcb_manager& operator=(tcb_manager&&)      = delete;

        std::optional<tcp_packet_t> gather_packet() {
                while (!active_tcbs_->empty()) {
                        std::optional<std::shared_ptr<tcb_t>> tcb = active_tcbs_->pop_front();
                        if (!tcb) continue;
                        std::optional<tcp_packet_t> tcp_packet = tcb.value()->gather_packet();
                        if (tcp_packet) return tcp_packet;
                }
                return std::nullopt;
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

        void receive(tcp_packet_t in_packet) {
                two_ends_t const two_end = {
                        .remote_info = in_packet.remote_info,
                        .local_info  = in_packet.local_info,
                };

                if (auto tcb_it{tcbs_.find(two_end)}; tcbs_.end() != tcb_it) {
                        tcp_transmit::tcp_in(tcb_it->second, in_packet);
                } else if (bound_.find(in_packet.local_info) != bound_.end()) {
                        register_tcb(two_end, listeners_[in_packet.local_info]);
                        if (auto tcb_it{tcbs_.find(two_end)}; tcbs_.end() != tcb_it) {
                                tcb_it->second->state      = TCP_LISTEN;
                                tcb_it->second->next_state = TCP_LISTEN;
                                tcp_transmit::tcp_in(tcb_it->second, in_packet);
                        } else {
                                spdlog::error("[TCB MNGR] fail register");
                        }
                } else {
                        spdlog::warn("[TCB MNGR] receive unknown tcp packet");
                }
        }

private:
        void register_tcb(two_ends_t const& two_end, std::shared_ptr<listener_t> listener) {
                assert(listener);
                spdlog::debug("[TCB MNGR] reg {}", two_end);
                tcbs_[two_end] = std::make_shared<tcb_t>(active_tcbs_, std::move(listener),
                                                         two_end.remote_info, two_end.local_info);
        }
};

}  // namespace mstack
