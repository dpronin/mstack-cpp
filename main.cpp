#include <cassert>
#include <cstdlib>

#include <array>
#include <charconv>
#include <print>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/system/detail/error_code.hpp>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include "mstack/acceptor.hpp"
#include "mstack/ipv4_addr.hpp"
#include "mstack/netns.hpp"
#include "mstack/socket.hpp"
#include "mstack/tap.hpp"
#include "mstack/tcp.hpp"
#include "mstack/tun.hpp"
#include "mstack/write.hpp"

namespace {

using namespace std::string_view_literals;

constexpr auto kBindIPv4AddrDefault{"192.168.0.1"sv};

void async_read_and_echo(std::shared_ptr<mstack::socket>              sk,
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

void do_accept(std::shared_ptr<mstack::acceptor> acceptor) {
        auto  sk{std::make_shared<mstack::socket>()};
        auto* p_acceptor{acceptor.get()};
        auto* p_sk{sk.get()};
        p_acceptor->async_accept(*p_sk, [acceptor = std::move(acceptor), sk = std::move(sk)](
                                                boost::system::error_code const& ec) mutable {
                if (ec) return;
                async_read_and_echo(std::move(sk), std::make_shared<std::array<std::byte, 2000>>());
                do_accept(std::move(acceptor));
        });
}

int tun_go(int argc, char const* argv[]) {
        auto& net{mstack::netns::_default_()};
        auto& io_ctx{net.io_context_execution()};

        auto dev{mstack::tun{}};

        auto bind_ipv4port{std::string_view{argv[0]}};

        auto bind_ipv4{bind_ipv4port.substr(0, bind_ipv4port.find(':'))};
        bind_ipv4port.remove_prefix(std::min(bind_ipv4port.size(), bind_ipv4.size() + 1));

        if (bind_ipv4.empty()) bind_ipv4 = kBindIPv4AddrDefault;

        auto bind_port{bind_ipv4port};
        bind_ipv4port.remove_prefix(std::min(bind_ipv4port.size(), bind_port.size() + 1));

        uint16_t port{0};
        std::from_chars(bind_port.data(), bind_port.data() + bind_port.size(), port);

        do_accept(std::make_unique<mstack::acceptor>(
                mstack::endpoint{mstack::tcp::PROTO, {mstack::ipv4_addr_t{bind_ipv4}, port}}));

        io_ctx.run();

        return EXIT_SUCCESS;
}

int tap_go(int argc, char const* argv[]) {
        auto& net{mstack::netns::_default_()};
        auto& io_ctx{net.io_context_execution()};

        auto dev{mstack::tap{}};

        auto bind_ipv4port{std::string_view{argv[0]}};

        auto bind_ipv4{bind_ipv4port.substr(0, bind_ipv4port.find(':'))};
        bind_ipv4port.remove_prefix(std::min(bind_ipv4port.size(), bind_ipv4.size() + 1));

        if (bind_ipv4.empty()) bind_ipv4 = kBindIPv4AddrDefault;

        dev.set_ipv4_addr(bind_ipv4);

        auto bind_port{bind_ipv4port};
        bind_ipv4port.remove_prefix(std::min(bind_ipv4port.size(), bind_port.size() + 1));

        uint16_t port{0};
        std::from_chars(bind_port.data(), bind_port.data() + bind_port.size(), port);

        do_accept(std::make_unique<mstack::acceptor>(
                mstack::endpoint{mstack::tcp::PROTO, {mstack::ipv4_addr_t{bind_ipv4}, port}}));

        io_ctx.run();

        return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char const* argv[]) {
        spdlog::set_level(spdlog::level::debug);

        --argc;
        ++argv;

        if (argc < 1) return EXIT_FAILURE;

        if (auto type{std::string_view{argv[0]}}; "TUN" == type) {
                return tun_go(argc - 1, ++argv);
        } else if ("TAP" == type) {
                return tap_go(argc - 1, ++argv);
        } else {
                spdlog::critical("invalid type '{}' given", type);
                return EXIT_FAILURE;
        }
}
