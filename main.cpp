#include <cassert>
#include <cstdlib>

#include <any>
#include <array>
#include <charconv>
#include <print>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/detail/error_code.hpp>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include "mstack/acceptor.hpp"
#include "mstack/api.hpp"
#include "mstack/ethernet.hpp"
#include "mstack/ipv4.hpp"
#include "mstack/packets.hpp"
#include "mstack/socket.hpp"
#include "mstack/tap.hpp"
#include "mstack/tun.hpp"
#include "mstack/write.hpp"

namespace {

void async_read_and_echo(std::shared_ptr<mstack::socket_t>            sk,
                         std::shared_ptr<std::array<std::byte, 2000>> buf) {
        auto* p_sk{sk.get()};
        auto* p_buf{buf.get()};
        p_sk->async_read_some(*p_buf, [sk = std::move(sk), buf = std::move(buf)](
                                              boost::system::error_code const& ec,
                                              size_t                           nbytes) mutable {
                if (ec) return;
                auto const msg{
                        std::string_view{
                                reinterpret_cast<char const*>(buf->data()),
                                static_cast<size_t>(nbytes),
                        },
                };
                std::println("read size: {}", msg.size());
                std::println("{}", msg);
                std::println("echoing...");
                auto* p_sk{sk.get()};
                mstack::async_write(
                        *p_sk, std::as_bytes(std::span{msg}),
                        [sk = std::move(sk), buf = std::move(buf)](
                                boost::system::error_code const& ec, size_t nbytes) mutable {
                                if (ec) return;
                                async_read_and_echo(std::move(sk), std::move(buf));
                        });
        });
}

void do_accept(boost::asio::io_context& io_ctx, std::shared_ptr<mstack::acceptor> acceptor) {
        auto  sk{std::make_shared<mstack::socket_t>(io_ctx)};
        auto* p_acceptor{acceptor.get()};
        auto* p_sk{sk.get()};
        p_acceptor->async_accept(*p_sk, [&io_ctx, acceptor = std::move(acceptor),
                                         sk = std::move(sk)](
                                                boost::system::error_code const& ec) mutable {
                if (ec) return;
                async_read_and_echo(std::move(sk), std::make_shared<std::array<std::byte, 2000>>());
                do_accept(io_ctx, std::move(acceptor));
        });
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

}  // namespace

int main(int argc, char* argv[]) {
        if (argc < 3) return EXIT_FAILURE;

        spdlog::set_level(spdlog::level::debug);

        mstack::init_stack();

        boost::asio::io_context io_ctx;

        std::any dev;

        if (!(argc < 4) && std::string_view{argv[1]} == "TUN") {
                auto tundev{std::make_shared<mstack::tun>(io_ctx)};
                tundev->capture(std::string_view{argv[2]});
                async_dev_read(tundev,
                               std::make_unique_for_overwrite<std::array<std::byte, 1500>>(),
                               [](std::span<std::byte> buf) {
                                       mstack::ipv4::instance().receive(mstack::raw_packet{
                                               .buffer = std::make_unique<mstack::base_packet>(buf),
                                       });
                               });
                async_dev_write_tick(tundev,
                                     std::make_unique_for_overwrite<std::array<std::byte, 1500>>(),
                                     [] { return mstack::ipv4::instance().gather_packet(); });
        } else {
                auto tapdev{std::make_shared<mstack::tap>(io_ctx)};
                tapdev->capture(std::string_view{argv[2]});
                tapdev->set_ipv4_addr(std::string_view{argv[2]});
                async_dev_read(tapdev,
                               std::make_unique_for_overwrite<std::array<std::byte, 1500>>(),
                               [](std::span<std::byte> buf) {
                                       mstack::ethernetv2::instance().receive(mstack::raw_packet{
                                               .buffer = std::make_unique<mstack::base_packet>(buf),
                                       });
                               });
                async_dev_write_tick(tapdev,
                                     std::make_unique_for_overwrite<std::array<std::byte, 1500>>(),
                                     [] { return mstack::ethernetv2::instance().gather_packet(); });
        }

        uint16_t port{0};
        std::from_chars(argv[3], argv[3] + strlen(argv[3]), port);

        do_accept(io_ctx, std::make_unique<mstack::acceptor>(
                                  io_ctx, mstack::tcp::PROTO,
                                  mstack::endpoint{{mstack::ipv4_addr_t{argv[2]}, port}}));

        io_ctx.run();
}
