#pragma once

#include <cstddef>

#include <chrono>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <spdlog/spdlog.h>

#include "arp.hpp"
#include "ethernet.hpp"
#include "icmp.hpp"
#include "ipv4.hpp"
#include "tcb_manager.hpp"
#include "tcp.hpp"

namespace mstack {

inline auto& tcp_stack_create() {
        auto& stack{tcp::instance()};

        auto& tcb_manager{tcb_manager::instance()};
        stack.register_upper_protocol(tcb_manager);

        return stack;
}

inline auto& icmp_stack_create() {
        auto& stack{icmp::instance()};
        return stack;
}

inline auto& ipv4_stack_create() {
        auto& stack{ipv4::instance()};

        stack.register_upper_protocol(icmp::instance());
        stack.register_upper_protocol(tcp::instance());

        return stack;
}

inline auto& arpv4_stack_create() {
        auto& stack{arp::instance()};
        return stack;
}

inline auto& ethernetv2_stack_create() {
        auto& stack{ethernetv2::instance()};

        stack.register_upper_protocol(ipv4::instance());
        stack.register_upper_protocol(arp::instance());

        return stack;
}

inline void init_stack() {
        tcp_stack_create();
        icmp_stack_create();
        ipv4_stack_create();
        arpv4_stack_create();
        ethernetv2_stack_create();
}

template <typename Device, size_t N, typename Receiver>
void async_dev_read(std::shared_ptr<Device>                   tundev,
                    std::unique_ptr<std::array<std::byte, N>> buf,
                    Receiver                                  receiver) {
        auto* p_dev{tundev.get()};
        auto* p_buf{buf.get()};
        p_dev->async_read_some(*p_buf, [tundev = std::move(tundev), receiver = std::move(receiver),
                                        buf = std::move(buf)](boost::system::error_code const& ec,
                                                              size_t nbytes) mutable {
                if (ec) {
                        spdlog::error("[RECEIVE FAIL] {}", ec.what());
                        return;
                }
                spdlog::debug("[RECEIVE] {}", nbytes);
                receiver({buf->data(), nbytes});
                async_dev_read(std::move(tundev), std::move(buf), std::move(receiver));
        });
}

template <typename Device, size_t N, typename Fetcher>
void async_dev_write_tick(std::shared_ptr<Device>                   tundev,
                          std::unique_ptr<std::array<std::byte, N>> buf,
                          Fetcher                                   fetcher) {
        if (auto pkt{fetcher()}) {
                auto          raw{mstack::raw_packet{.buffer = std::move(pkt->buffer)}};
                ssize_t const len{raw.buffer->export_data(*buf)};
                auto*         p_dev{tundev.get()};
                auto*         p_buf{buf.get()};
                p_dev->async_write(
                        {p_buf->data(), static_cast<size_t>(len)},
                        [tundev = std::move(tundev), buf = std::move(buf),
                         fetcher = std::move(fetcher)](boost::system::error_code const& ec,
                                                       size_t nbytes [[maybe_unused]]) mutable {
                                if (ec) {
                                        spdlog::error("[WRITE FAIL] {}", ec.what());
                                        return;
                                }
                                async_dev_write_tick(std::move(tundev), std::move(buf),
                                                     std::move(fetcher));
                        });
        } else {
                using namespace std::chrono_literals;
                auto t{std::make_unique<boost::asio::steady_timer>(tundev->get_executor())};
                t->expires_after(100ms);
                auto* p_t{t.get()};
                p_t->async_wait([tundev = std::move(tundev), buf = std::move(buf),
                                 fetcher = std::move(fetcher),
                                 t = std::move(t)](boost::system::error_code const& ec) mutable {
                        if (ec) {
                                spdlog::error("[WRITE TICK FAIL] {}", ec.what());
                                return;
                        }
                        async_dev_write_tick(std::move(tundev), std::move(buf), std::move(fetcher));
                });
        }
}

}  // namespace mstack
