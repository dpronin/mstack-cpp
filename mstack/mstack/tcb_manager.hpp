#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "circle_buffer.hpp"
#include "defination.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcb.hpp"
#include "tcp_transmit.hpp"

namespace mstack {

class tcb_manager {
private:
        std::unordered_set<uint16_t>                                 sockets_;
        std::shared_ptr<circle_buffer<std::shared_ptr<tcb_t>>>       active_tcbs_;
        std::unordered_map<two_ends_t, std::shared_ptr<tcb_t>>       tcbs_;
        std::unordered_set<ipv4_port_t>                              active_ports_;
        std::unordered_map<ipv4_port_t, std::shared_ptr<listener_t>> listeners_;

public:
        constexpr static int PROTO{0x06};

        tcb_manager() : active_tcbs_(std::make_shared<circle_buffer<std::shared_ptr<tcb_t>>>()) {}
        ~tcb_manager() = default;

        tcb_manager(const tcb_manager&) = delete;
        tcb_manager(tcb_manager&&)      = delete;

        tcb_manager& operator=(const tcb_manager&) = delete;
        tcb_manager& operator=(tcb_manager&&)      = delete;

        auto const& sockets() const { return sockets_; }
        auto&       sockets() { return sockets_; }

        std::optional<tcp_packet_t> gather_packet() {
                while (!active_tcbs_->empty()) {
                        std::optional<std::shared_ptr<tcb_t>> tcb = active_tcbs_->pop_front();
                        if (!tcb) continue;
                        std::optional<tcp_packet_t> tcp_packet = tcb.value()->gather_packet();
                        if (tcp_packet) return tcp_packet;
                }
                return std::nullopt;
        }

        listener_t& listener_get(ipv4_port_t const& ipv4_port) {
                return *this->listeners_[ipv4_port];
        }

        void listen_port(ipv4_port_t const& ipv4_port, std::shared_ptr<listener_t> listener) {
                this->listeners_[ipv4_port] = std::move(listener);
                active_ports_.insert(ipv4_port);
        }

        void register_tcb(two_ends_t& two_end, std::shared_ptr<listener_t> listener) {
                spdlog::debug("[REGISTER TCB] {}", two_end);
                if (!two_end.remote_info || !two_end.local_info) {
                        spdlog::critical("[EMPTY TCB]");
                }
                tcbs_[two_end] = std::make_shared<tcb_t>(this->active_tcbs_, std::move(listener),
                                                         two_end.remote_info.value(),
                                                         two_end.local_info.value());
        }

        void receive(tcp_packet_t in_packet) {
                two_ends_t two_end = {
                        .remote_info = in_packet.remote_info,
                        .local_info  = in_packet.local_info,
                };
                if (tcbs_.find(two_end) != tcbs_.end()) {
                        tcp_transmit::tcp_in(tcbs_[two_end], in_packet);
                } else if (active_ports_.find(in_packet.local_info) != active_ports_.end()) {
                        register_tcb(two_end, this->listeners_[in_packet.local_info]);
                        if (tcbs_.find(two_end) != tcbs_.end()) {
                                tcbs_[two_end]->state      = TCP_LISTEN;
                                tcbs_[two_end]->next_state = TCP_LISTEN;
                                tcp_transmit::tcp_in(tcbs_[two_end], in_packet);
                        } else {
                                spdlog::error("[REGISTER TCB FAIL]");
                        }

                } else {
                        spdlog::warn("[RECEIVE UNKNOWN TCP PACKET]");
                }
        }
};

}  // namespace mstack
