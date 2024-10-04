#include "device.hpp"

#include <cassert>

#include <algorithm>
#include <array>
#include <memory>

#include <linux/if_tun.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include "file_desc.hpp"
#include "ipv4_addr.hpp"
#include "mac_addr.hpp"
#include "netns.hpp"
#include "skbuff.hpp"
#include "utils.hpp"

namespace mstack {

template <typename Completion>
void device::async_read_some(std::span<std::byte> buf, Completion&& completion) {
        spdlog::debug("[DEV {}]: READ SOME MAX {} BYTES", ndev_, buf.size());
        pfd_.async_read_some(boost::asio::buffer(buf), std::forward<Completion>(completion));
}

template <typename Completion>
void device::async_write(std::span<std::byte const> buf, Completion&& completion) {
        spdlog::debug("[DEV {}]: WRITE EXACTLY {} BYTES", ndev_, buf.size());
        boost::asio::async_write(pfd_, boost::asio::buffer(buf),
                                 std::forward<Completion>(completion));
}

void device::send_front_pkt_out() {
        auto skb{std::move(out_skb_q_.front())};
        auto buf{skb.payload()};
        async_write(buf, [this, skb = std::move(skb)](boost::system::error_code const& ec,
                                                      size_t nbytes [[maybe_unused]]) mutable {
                if (ec) {
                        spdlog::error("[DEV {}] WRITE FAIL {}", ndev_, ec.what());
                        return;
                }
                out_skb_q_.pop();
                if (!out_skb_q_.empty()) send_front_pkt_out();
        });
}

void device::process(skbuff&& skb) {
        out_skb_q_.push(std::move(skb));
        if (1 == out_skb_q_.size()) send_front_pkt_out();
}

void device::async_receive() {
        auto  buf{std::make_unique_for_overwrite<std::byte[]>(1500)};
        auto* p_buf{buf.get()};
        async_read_some(
                {p_buf, 1500}, [this, buf = std::move(buf)](boost::system::error_code const& ec,
                                                            size_t nbytes) mutable {
                        if (ec) {
                                spdlog::error("[DEV {}] RECEIVE FAIL {}", ndev_, ec.what());
                                return;
                        }
                        spdlog::debug("[DEV {}] RECEIVE {}", ndev_, nbytes);
                        net_.eth().receive(skbuff{std::move(buf), nbytes}, shared_from_this());
                        async_receive();
                });
}

device::device(netns& net /* = netns::_default_()*/, std::string_view name /* = ""*/)
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

        mac_addr_ = mac_addr_t::generate();

        spdlog::debug("[DEV {}]: MAC ADDRESS {} IS SET", ndev_, mac_addr_);

        pfd_.assign(fd_.get_fd());

        async_receive();
}

std::string const& device::name() const { return ndev_; }

auto& device::get_executor() { return pfd_.get_executor(); }

std::optional<ipv4_addr_t> device::ipv4_addr() const { return ipv4_addr_; }

void device::set_ipv4_addr(ipv4_addr_t const& ipv4_addr) {
        reset_ipv4_addr();
        ipv4_addr_ = ipv4_addr;
        net_.arp_cache().update({ipv4_addr, mac_addr_});
}

void device::reset_ipv4_addr() {
        if (ipv4_addr_) {
                net_.arp_cache().reset(*ipv4_addr_);
                net_.rt().reset(*ipv4_addr_);
                ipv4_addr_.reset();
        }
}

netns&       device::net() { return net_; }
netns const& device::net() const { return net_; }

mac_addr_t const& device::mac_addr() const { return mac_addr_; }

}  // namespace mstack