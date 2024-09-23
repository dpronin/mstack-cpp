#include "tcb_manager.hpp"

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

tcb_manager::tcb_manager(boost::asio::io_context& io_ctx)
    : base_protocol(io_ctx),
      active_tcbs_(std::make_shared<circle_buffer<std::shared_ptr<tcb_t>>>()) {}

void tcb_manager::activate() {
        while (!active_tcbs_->empty()) {
                if (auto tcb{active_tcbs_->pop()}) {
                        if (auto tcp_pkt{tcb.value()->gather_packet()}) {
                                enqueue(std::move(*tcp_pkt));
                        }
                }
        }
}

std::shared_ptr<listener_t> tcb_manager::listener_get(ipv4_port_t const& ipv4_port) {
        return listeners_[ipv4_port];
}

void tcb_manager::bind(ipv4_port_t const& ipv4_port) {
        if (!bound_.insert(ipv4_port).second)
                throw std::system_error(std::make_error_code(std::errc::address_in_use));
}

void tcb_manager::listen(ipv4_port_t const& ipv4_port, std::shared_ptr<listener_t> listener) {
        if (!bound_.contains(ipv4_port)) {
                throw std::system_error(std::make_error_code(std::errc::address_not_available));
        }
        assert(listener);
        listeners_[ipv4_port] = std::move(listener);
}

void tcb_manager::process(tcp_packet_t&& in_pkt) {
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
                        std::make_shared<tcb_t>(io_ctx_, *this, active_tcbs_,
                                                listeners_[in_pkt.local_info], two_end.remote_info,
                                                two_end.local_info));
                tcb_it->second->state_      = TCP_LISTEN;
                tcb_it->second->next_state_ = TCP_LISTEN;
                tcp_transmit::tcp_in(tcb_it->second, std::move(in_pkt));
        } else {
                spdlog::warn("[TCB MNGR] receive unknown TCP packet");
        }

        activate();
}

}  // namespace mstack
