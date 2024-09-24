#include "tcb_manager.hpp"

#include <cassert>

#include <memory>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include <boost/asio/io_context.hpp>

#include <spdlog/spdlog.h>

#include "base_protocol.hpp"
#include "defination.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcb.hpp"

namespace mstack {

tcb_manager::tcb_manager(boost::asio::io_context& io_ctx) : base_protocol(io_ctx) {}

std::shared_ptr<listener_t> tcb_manager::listener_get(ipv4_port_t const& ipv4_port) {
        return listeners_[ipv4_port];
}

void tcb_manager::bind(ipv4_port_t const& ipv4_port) {
        if (!bound_.insert(ipv4_port).second)
                throw std::system_error(std::make_error_code(std::errc::address_in_use));
}

void tcb_manager::listen(ipv4_port_t const& ipv4_port, std::shared_ptr<listener_t> listener) {
        if (!bound_.contains(ipv4_port))
                throw std::system_error(std::make_error_code(std::errc::address_not_available));
        assert(listener);
        listeners_[ipv4_port] = std::move(listener);
}

void tcb_manager::process(tcp_packet_t&& in_pkt) {
        two_ends_t const two_end = {
                .remote_info = in_pkt.remote_info,
                .local_info  = in_pkt.local_info,
        };

        tcb_t* p_tcb{nullptr};

        if (auto tcb_it{tcbs_.find(two_end)}; tcbs_.end() != tcb_it) {
                p_tcb = tcb_it->second.get();
        } else if (auto listener{listeners_.find(in_pkt.local_info)};
                   listeners_.end() != listener) {
                spdlog::debug("[TCB MNGR] reg {}", two_end);

                tcb_it = tcbs_.emplace_hint(
                        tcb_it, two_end,
                        tcb_t::create_shared(io_ctx_, *this, listener->second, two_end.remote_info,
                                             two_end.local_info, listener->second->proto,
                                             TCP_LISTEN, TCP_LISTEN));

                p_tcb = tcb_it->second.get();
        } else {
                spdlog::warn("[TCB MNGR] receive unknown TCP packet");
        }

        if (p_tcb) p_tcb->process(std::move(in_pkt));
}

}  // namespace mstack
