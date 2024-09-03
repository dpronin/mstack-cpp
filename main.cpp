#include <cstdlib>

#include <any>
#include <charconv>
#include <future>
#include <iostream>
#include <syncstream>

#include <boost/asio/io_context.hpp>

#include "mstack/api.hpp"

int main(int argc, char* argv[]) {
        if (argc < 3) return EXIT_FAILURE;

        mstack::init_stack();

        boost::asio::io_context io_ctx;

        std::any dev;

        if (!(argc < 4) && std::string_view{argv[3]} == "TUN") {
                auto tundev{mstack::tun_dev_create(io_ctx)};
                tundev->set_ipv4_addr(std::string_view{argv[1]});
                tundev->capture("192.168.1.0/24");
                dev = std::shared_ptr{std::move(tundev)};
        } else {
                auto tapdev{mstack::tap_dev_create<1500>(io_ctx)};
                tapdev->set_ipv4_addr(std::string_view{argv[1]});
                tapdev->capture("192.168.1.0/24");
                dev = std::shared_ptr{std::move(tapdev)};
        }

        uint16_t port{0};
        std::from_chars(argv[2], argv[2] + strlen(argv[2]), port);

        int const fd{mstack::socket(mstack::tcp::PROTO, mstack::ipv4_addr_t(argv[1]), port)};
        mstack::listen(fd);

        auto stack{std::async(std::launch::async, [&io_ctx] { io_ctx.run(); })};

        for (std::vector<std::future<void>> workers;;) {
                int const cfd{mstack::accept(fd)};
                workers.push_back(std::async(
                        std::launch::async,
                        [](int cfd) {
                                while (true) {
                                        std::array<std::byte, 2000> buf;

                                        if (ssize_t r{mstack::read(cfd, buf)}; !(r < 0)) {
                                                auto const msg{
                                                        std::string_view{
                                                                reinterpret_cast<char const*>(
                                                                        buf.data()),
                                                                static_cast<size_t>(r),
                                                        },
                                                };
                                                std::osyncstream osync{std::cout};
                                                osync << "read size: " << msg.size() << std::endl;
                                                osync << msg << std::endl;
                                                osync << std::endl;
                                        }
                                }
                        },
                        cfd));
        }
}
