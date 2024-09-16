#pragma once

#include <cstddef>

#include <array>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <spdlog/spdlog.h>

#include "raw_packet.hpp"

namespace mstack {

template <typename Device, size_t N, typename Receiver>
void async_dev_read(Device&                                   dev,
                    std::unique_ptr<std::array<std::byte, N>> buf,
                    Receiver&&                                receiver) {
        auto* p_buf{buf.get()};
        dev.async_read_some(*p_buf,
                            [&dev, receiver = std::move(receiver), buf = std::move(buf)](
                                    boost::system::error_code const& ec, size_t nbytes) mutable {
                                    if (ec) {
                                            spdlog::error("[RECEIVE FAIL] {}", ec.what());
                                            return;
                                    }
                                    spdlog::debug("[RECEIVE] {}", nbytes);
                                    receiver({buf->data(), nbytes});
                                    async_dev_read(dev, std::move(buf), std::move(receiver));
                            });
}

template <typename Device, size_t N, typename Fetcher>
void async_dev_write_tick(Device&                                   dev,
                          std::unique_ptr<std::array<std::byte, N>> buf,
                          Fetcher&&                                 fetcher) {
        if (auto pkt{fetcher()}) {
                auto          raw{mstack::raw_packet{.buffer = std::move(pkt->buffer)}};
                ssize_t const len{raw.buffer->export_data(*buf)};
                auto*         p_buf{buf.get()};
                dev.async_write({p_buf->data(), static_cast<size_t>(len)},
                                [&dev, buf = std::move(buf), fetcher = std::move(fetcher)](
                                        boost::system::error_code const& ec,
                                        size_t nbytes [[maybe_unused]]) mutable {
                                        if (ec) {
                                                spdlog::error("[WRITE FAIL] {}", ec.what());
                                                return;
                                        }
                                        async_dev_write_tick(dev, std::move(buf),
                                                             std::move(fetcher));
                                });
        } else {
                using namespace std::chrono_literals;
                auto t{
                        std::make_unique<boost::asio::steady_timer>(
                                dev.net().io_context_execution()),
                };
                t->expires_after(100ms);
                auto* p_t{t.get()};
                p_t->async_wait([&dev, buf = std::move(buf), fetcher = std::move(fetcher),
                                 t = std::move(t)](boost::system::error_code const& ec) mutable {
                        if (ec) {
                                spdlog::error("[WRITE TICK FAIL] {}", ec.what());
                                return;
                        }
                        async_dev_write_tick(dev, std::move(buf), std::move(fetcher));
                });
        }
}

}  // namespace mstack
