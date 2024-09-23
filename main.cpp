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

}  // namespace

int main(int argc, char const* argv[]) {
        spdlog::set_level(spdlog::level::debug);

        --argc;
        ++argv;

        auto bind_devipv4port{std::string_view{argv[0]}};

        auto dev_name{bind_devipv4port.substr(0, bind_devipv4port.find(':'))};
        bind_devipv4port.remove_prefix(std::min(bind_devipv4port.size(), dev_name.size() + 1));

        auto& netns{mstack::netns::_default_()};

        auto dev{mstack::tap::create(netns, dev_name)};

        auto bind_ipv4{bind_devipv4port.substr(0, bind_devipv4port.find(':'))};
        bind_devipv4port.remove_prefix(std::min(bind_devipv4port.size(), bind_ipv4.size() + 1));

        if (bind_ipv4.empty()) bind_ipv4 = kBindIPv4AddrDefault;

        dev->set_ipv4_addr(bind_ipv4);

        auto bind_port{bind_devipv4port};
        bind_devipv4port.remove_prefix(std::min(bind_devipv4port.size(), bind_port.size() + 1));

        uint16_t port{0};
        std::from_chars(bind_port.data(), bind_port.data() + bind_port.size(), port);

        do_accept(std::make_unique<mstack::acceptor>(mstack::endpoint{
                mstack::tcp::PROTO,
                {mstack::ipv4_addr_t{bind_ipv4}, port},
        }));

        netns.rt().update_default({
                .addr = mstack::ipv4_addr_t{std::string_view{argv[1]}},
                .dev  = dev,
        });

        netns.io_context_execution().run();

        return EXIT_SUCCESS;
}
