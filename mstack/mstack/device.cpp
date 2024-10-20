#include "device.hpp"

#include <cassert>

#include <algorithm>
#include <array>
#include <memory>
#include <tuple>

#include <linux/if_tun.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/write.hpp>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "file_desc.hpp"
#include "netns.hpp"
#include "skbuff.hpp"

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
                if (ec) spdlog::warn("[DEV {}] WRITE FAIL {}", ndev_, ec.what());
                out_skb_q_.pop();
                if (!out_skb_q_.empty()) send_front_pkt_out();
        });
}

void device::process(skbuff&& skb_in) {
        out_skb_q_.push(std::move(skb_in));
        if (1 == out_skb_q_.size()) send_front_pkt_out();
}

void device::async_receive() {
        auto  buf{std::make_unique_for_overwrite<std::byte[]>(1500)};
        auto* p_buf{buf.get()};
        async_read_some({p_buf, 1500},
                        [this, buf = std::move(buf)](boost::system::error_code const& ec,
                                                     size_t nbytes) mutable {
                                if (ec) {
                                        spdlog::warn("[DEV {}] RECEIVE FAIL {}", ndev_, ec.what());
                                        async_receive();
                                }
                                spdlog::debug("[DEV {}] RECEIVE {}", ndev_, nbytes);
                                net_.eth().receive(skbuff{std::move(buf), 1500, 0, 1500 - nbytes},
                                                   shared_from_this());
                                async_receive();
                        });
}

device::device(netns& net /* = netns::_default_()*/, std::string_view name /* = ""*/)
    : net_(net), pfd_(net_.io_context_execution()) {
        auto fd{file_desc::open("/dev/net/tun", file_desc::RDWR | file_desc::NONBLOCK)};
        if (!fd) throw std::runtime_error{std::format("[TAP] OPEN FAIL")};

        spdlog::debug("[TAP] DEV FD {}", fd->get_fd());

        ifreq ifr{};

        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

        name = name.substr(0, std::min(name.size(), sizeof(ifr.ifr_name) - 1));
        std::ranges::copy(name, std::begin(ifr.ifr_name));

        if (int ec{fd->ioctl(TUNSETIFF, ifr)}; ec < 0)
                throw std::runtime_error{std::format("[TAP] SETUP FAIL")};

        ndev_.assign(ifr.ifr_name);

        pfd_.assign(fd->get_fd());

        async_receive();

        std::ignore = fd->release();
}

device::~device() noexcept { ::close(pfd_.native_handle()); }

std::string const& device::name() const { return ndev_; }

auto& device::get_executor() { return pfd_.get_executor(); }

netns&       device::net() { return net_; }
netns const& device::net() const { return net_; }

}  // namespace mstack
