#include "tap.hpp"

#include <cassert>

#include <algorithm>
#include <array>
#include <random>

#include <linux/if_tun.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include "file_desc.hpp"
#include "ipv4_addr.hpp"
#include "mac_addr.hpp"
#include "netns.hpp"
#include "raw_packet.hpp"
#include "utils.hpp"

namespace mstack {

template <typename Completion>
void tap::async_read_some(std::span<std::byte> buf, Completion&& completion) {
        spdlog::debug("[TAP {}]: READ SOME MAX {} BYTES", ndev_, buf.size());
        pfd_.async_read_some(boost::asio::buffer(buf), std::forward<Completion>(completion));
}

template <typename Completion>
void tap::async_write(std::span<std::byte const> buf, Completion&& completion) {
        spdlog::debug("[TAP {}]: WRITE EXACTLY {} BYTES", ndev_, buf.size());
        boost::asio::async_write(pfd_, boost::asio::buffer(buf),
                                 std::forward<Completion>(completion));
}

void tap::send_front_pkt_out() {
        auto          raw{std::move(out_queue_.front())};
        ssize_t const len{raw.buffer->export_data(out_buf_)};
        async_write({out_buf_.data(), static_cast<size_t>(len)},
                    [this](boost::system::error_code const& ec,
                           size_t                           nbytes [[maybe_unused]]) mutable {
                            if (ec) {
                                    spdlog::error("[TAP {}] WRITE FAIL {}", ndev_, ec.what());
                                    return;
                            }
                            out_queue_.pop();
                            if (!out_queue_.empty()) send_front_pkt_out();
                    });
}

void tap::process(raw_packet&& pkt) {
        out_queue_.push(std::move(pkt));
        if (1 == out_queue_.size()) send_front_pkt_out();
}

void tap::async_receive() {
        async_read_some(in_buf_, [this](boost::system::error_code const& ec, size_t nbytes) {
                if (ec) {
                        spdlog::error("[TAP {}] RECEIVE FAIL {}", ndev_, ec.what());
                        return;
                }
                spdlog::debug("[TAP {}] RECEIVE {}", ndev_, nbytes);
                net_.eth().receive(mstack::raw_packet{
                        .buffer = std::make_unique<mstack::base_packet>(
                                std::span{in_buf_.data(), nbytes}),
                });
                async_receive();
        });
}

tap::tap(netns& net /* = netns::_default_()*/, std::string_view name /* = ""*/)
    : net_(net), pfd_(net_.io_context_execution()) {
        auto fd{
                file_desc::open("/dev/net/tun", file_desc::RDWR | file_desc::NONBLOCK),
        };

        if (!fd) {
                spdlog::critical("[TAP] INIT FAIL");
                return;
        }

        fd_ = std::move(fd.value());

        spdlog::debug("[TAP] DEV FD {}", fd_.get_fd());

        ifreq ifr{};

        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

        name = name.substr(0, std::min(name.size(), sizeof(ifr.ifr_name) - 1));
        std::ranges::copy(name, std::begin(ifr.ifr_name));

        if (int err{fd_.ioctl(TUNSETIFF, ifr)}; err < 0) {
                spdlog::critical("[TAP] INIT FAIL");
                return;
        }

        ndev_ = std::string_view{ifr.ifr_name};

        if (utils::set_interface_up(ndev_) != 0) {
                spdlog::critical("[TAP] SET UP {}", ndev_);
                return;
        }

        std::array<std::byte, 6> hw_addr;
        std::generate(hw_addr.begin(), hw_addr.end(),
                      [gen  = std::mt19937{std::random_device{}()},
                       dist = std::uniform_int_distribution<uint8_t>{0x00, 0xff}] mutable {
                              return static_cast<std::byte>(dist(gen));
                      });
        hw_addr[0] = ((hw_addr[0] >> 1) | static_cast<std::byte>(0x1)) << 1;
        mac_addr_  = mac_addr_t{hw_addr};

        spdlog::debug("[TAP {}]: MAC ADDRESS {} IS SET", ndev_, mac_addr_);

        pfd_.assign(fd_.get_fd());

        net_.eth().under_handler_update(std::bind(&tap::process, this, std::placeholders::_1));

        async_receive();
}

std::string const& tap::name() const { return ndev_; }

auto& tap::get_executor() { return pfd_.get_executor(); }

std::optional<ipv4_addr_t> tap::ipv4_addr() const { return ipv4_addr_; }

void tap::set_ipv4_addr(ipv4_addr_t const& ipv4_addr) {
        reset_ipv4_addr();
        ipv4_addr_ = ipv4_addr;
        net_.arp_cache().update({ipv4_addr, mac_addr_});
}

void tap::reset_ipv4_addr() {
        if (ipv4_addr_) {
                net_.arp_cache().reset(*ipv4_addr_);
                net_.rt().reset(*ipv4_addr_);
                ipv4_addr_.reset();
        }
}

netns&       tap::net() { return net_; }
netns const& tap::net() const { return net_; }

mac_addr_t const& tap::mac_addr() const { return mac_addr_; }

}  // namespace mstack
