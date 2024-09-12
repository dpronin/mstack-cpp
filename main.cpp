#include <cassert>
#include <cstdlib>

#include <any>
#include <array>
#include <charconv>
#include <print>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/system/detail/error_code.hpp>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include "mstack/acceptor.hpp"
#include "mstack/api.hpp"
#include "mstack/socket.hpp"

namespace {

void async_read_and_echo(std::shared_ptr<mstack::socket_t>            csk,
                         std::shared_ptr<std::array<std::byte, 2000>> buf) {
        auto* p_csk{csk.get()};
        auto* p_buf{buf.get()};
        p_csk->async_read_some(*p_buf, [csk = std::move(csk), buf = std::move(buf)](
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
                auto* p_csk{csk.get()};
                p_csk->async_write(
                        std::as_bytes(std::span{msg}),
                        [csk = std::move(csk), buf = std::move(buf)](
                                boost::system::error_code const& ec, size_t nbytes) mutable {
                                if (ec) return;
                                async_read_and_echo(std::move(csk), std::move(buf));
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

}  // namespace

int main(int argc, char* argv[]) {
        if (argc < 3) return EXIT_FAILURE;

        spdlog::set_level(spdlog::level::debug);

        mstack::init_stack();

        boost::asio::io_context io_ctx;

        std::any dev;

        if (!(argc < 4) && std::string_view{argv[1]} == "TUN") {
                auto tundev{mstack::tun_dev_create(io_ctx)};
                tundev->capture(std::string_view{argv[2]});
                dev = std::shared_ptr{std::move(tundev)};
        } else {
                auto tapdev{mstack::tap_dev_create<1500>(io_ctx)};
                tapdev->set_ipv4_addr(std::string_view{argv[2]});
                tapdev->capture(std::string_view{argv[2]});
                dev = std::shared_ptr{std::move(tapdev)};
        }

        uint16_t port{0};
        std::from_chars(argv[3], argv[3] + strlen(argv[3]), port);

        do_accept(io_ctx, std::make_unique<mstack::acceptor>(
                                  io_ctx, mstack::tcp::PROTO,
                                  mstack::endpoint{mstack::ipv4_addr_t{argv[2]}, port}));

        io_ctx.run();
}
