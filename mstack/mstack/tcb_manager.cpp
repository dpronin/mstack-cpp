#include "tcb_manager.hpp"

#include <cassert>

#include <memory>
#include <random>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include <boost/asio/io_context.hpp>

#include <spdlog/spdlog.h>

#include "base_protocol.hpp"
#include "defination.hpp"
#include "endpoint.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcb.hpp"

namespace mstack {

class tcb_manager::port_generator_ctx {
public:
        port_generator_ctx() : gen{std::random_device{}()}, dist{1024, 49151} {}
        ~port_generator_ctx() = default;

        port_generator_ctx(port_generator_ctx const&)            = delete;
        port_generator_ctx& operator=(port_generator_ctx const&) = delete;

        port_generator_ctx(port_generator_ctx&&)            = delete;
        port_generator_ctx& operator=(port_generator_ctx&&) = delete;

        uint16_t generate() { return dist(gen); }

private:
        std::mt19937                            gen;
        std::uniform_int_distribution<uint16_t> dist;
};

tcb_manager::tcb_manager(boost::asio::io_context& io_ctx)
    : base_protocol(io_ctx), port_gen_ctx_(std::make_unique<port_generator_ctx>()) {}

tcb_manager::~tcb_manager() noexcept = default;

void tcb_manager::rule_insert_front(
        std::function<bool(ipv4_port_t const& remote_info, ipv4_port_t const& local_info)> matcher,
        int                                                                                proto,
        std::function<void(boost::system::error_code const& ec,
                           ipv4_port_t const&               remote_info,
                           ipv4_port_t const&               local_info,
                           std::weak_ptr<tcb_t>)>                                          cb) {
        rules_.emplace_front(std::move(matcher), proto, std::move(cb));
}

void tcb_manager::rule_insert_back(
        std::function<bool(ipv4_port_t const& remote_info, ipv4_port_t const& local_info)> matcher,
        int                                                                                proto,
        std::function<void(boost::system::error_code const& ec,
                           ipv4_port_t const&               remote_info,
                           ipv4_port_t const&               local_info,
                           std::weak_ptr<tcb_t>)>                                          cb) {
        rules_.emplace_back(std::move(matcher), proto, std::move(cb));
}

void tcb_manager::async_connect(endpoint const&                           ep,
                                std::function<void(boost::system::error_code const& ec,
                                                   ipv4_port_t const&               remote_info,
                                                   ipv4_port_t const&               local_info,
                                                   std::weak_ptr<tcb_t>)> cb) {
        while (true) {
                two_ends_t const two_end = {
                        .remote_info = ep.ep(),
                        .local_info  = {0x0a0a0a02, port_gen_ctx_->generate()},
                };

                auto [tcb_it, created] = tcbs_.emplace(
                        two_end,
                        tcb_t::create_shared(io_ctx_, *this, two_end.remote_info,
                                             two_end.local_info, ep.proto(), kTCPConnecting,
                                             kTCPConnecting, std::move(cb)));
                if (!created) continue;

                spdlog::debug("[TCB MNGR] new connect {}", two_end);

                tcb_it->second->start_connecting();

                break;
        }
}

void tcb_manager::process(tcp_packet&& in_pkt) {
        two_ends_t const two_end = {
                .remote_info = in_pkt.remote_info,
                .local_info  = in_pkt.local_info,
        };

        spdlog::debug("[TCB MNGR] RECEIVE {}", two_end);

        tcb_t* p_tcb{nullptr};

        if (auto tcb_it{tcbs_.find(two_end)}; tcbs_.end() != tcb_it) {
                p_tcb = tcb_it->second.get();
        } else {
                for (auto const& [matcher, proto, cb] : rules_) {
                        if (matcher(two_end.remote_info, two_end.local_info)) {
                                spdlog::debug("[TCB MNGR] reg {}", two_end);

                                tcb_it = tcbs_.emplace_hint(
                                        tcb_it, two_end,
                                        tcb_t::create_shared(io_ctx_, *this, two_end.remote_info,
                                                             two_end.local_info, proto, kTCPListen,
                                                             kTCPListen, cb));

                                p_tcb = tcb_it->second.get();

                                break;
                        }
                }
        }

        if (p_tcb) {
                spdlog::debug("[TCB MNGR] HANDLE {}", two_end);
                p_tcb->process(std::move(in_pkt));
        } else {
                spdlog::warn("[TCB MNGR] UNKNOWN INPUT {}", two_end);
        }
}

}  // namespace mstack
