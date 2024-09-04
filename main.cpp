#include <cassert>
#include <cstdlib>

#include <any>
#include <array>
#include <charconv>
#include <iostream>
#include <syncstream>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/system/detail/error_code.hpp>

#include "mstack/api.hpp"

namespace {

void async_read(std::shared_ptr<mstack::socket_t>            csk,
                std::shared_ptr<std::array<std::byte, 2000>> buf) {
        auto* p_csk{csk.get()};
        auto* p_buf{buf.get()};
        p_csk->async_read_some(*p_buf, [csk = std::move(csk), buf = std::move(buf)](
                                               boost::system::error_code const& ec, size_t nbytes) {
                if (ec) return;
                auto const msg{
                        std::string_view{
                                reinterpret_cast<char const*>(buf->data()),
                                static_cast<size_t>(nbytes),
                        },
                };
                std::osyncstream osync{std::cout};
                osync << "read size: " << msg.size() << std::endl;
                osync << msg << std::endl;
                osync << std::endl;
                async_read(std::move(csk), std::move(buf));
        });
}

void do_accept(boost::asio::io_context& io_ctx, int fd) {
        mstack::async_accept(
                io_ctx, fd, [&io_ctx, fd](auto const& ec, std::shared_ptr<mstack::socket_t> csk) {
                        assert(csk);
                        async_read(std::move(csk), std::make_shared<std::array<std::byte, 2000>>());
                        do_accept(io_ctx, fd);
                });
}

}  // namespace

int main(int argc, char* argv[]) {
        if (argc < 3) return EXIT_FAILURE;

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
                tapdev->capture("192.168.1.0/24");
                dev = std::shared_ptr{std::move(tapdev)};
        }

        uint16_t port{0};
        std::from_chars(argv[3], argv[3] + strlen(argv[3]), port);

        int const fd{
                mstack::socket(io_ctx, mstack::tcp::PROTO, mstack::ipv4_addr_t{argv[2]}, port),
        };
        mstack::listen(fd);

        do_accept(io_ctx, fd);

        io_ctx.run();
}
