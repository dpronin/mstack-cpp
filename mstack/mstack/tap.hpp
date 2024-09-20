#pragma once

#include <linux/if_tun.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <cassert>
#include <optional>
#include <span>
#include <string>

#include "file_desc.hpp"
#include "ipv4_addr.hpp"
#include "mac_addr.hpp"
#include "mstack/raw_packet.hpp"
#include "netns.hpp"
#include "utils.hpp"

namespace mstack {

class tap {
private:
        netns&                                net_;
        boost::asio::posix::stream_descriptor pfd_;
        file_desc                             fd_;
        mac_addr_t                            mac_addr_;
        std::optional<ipv4_addr_t>            ipv4_addr_;
        std::string                           ndev_;
        std::queue<raw_packet>                out_queue_;

        std::array<std::byte, 1500> in_buf_;
        std::array<std::byte, 1500> out_buf_;

        void set_mac_addr() {
                ifreq ifr{};

                strcpy(ifr.ifr_name, ndev_.data());

                if (int err{fd_.ioctl(SIOCGIFHWADDR, ifr)}; err < 0) {
                        throw std::runtime_error(
                                std::format("failed to set MAC address, [HW FAIL], err {}", err));
                }

                std::array<std::byte, 6> hw_addr;
                for (int i = 0; i < 6; ++i)
                        hw_addr[i] = static_cast<std::byte>(ifr.ifr_addr.sa_data[i]);

                mac_addr_ = mac_addr_t{hw_addr};
        }

        template <typename Completion>
        void async_read_some(std::span<std::byte> buf, Completion&& completion) {
                spdlog::debug("[TAP READ SOME]: max {} bytes", buf.size());
                pfd_.async_read_some(boost::asio::buffer(buf),
                                     std::forward<Completion>(completion));
        }

        template <typename Completion>
        void async_write(std::span<std::byte const> buf, Completion&& completion) {
                spdlog::debug("[TAP WRITE]: exactly {} bytes", buf.size());
                boost::asio::async_write(pfd_, boost::asio::buffer(buf),
                                         std::forward<Completion>(completion));
        }

        void send_front_pkt_out() {
                auto raw{mstack::raw_packet{.buffer = std::move(out_queue_.front().buffer)}};
                ssize_t const len{raw.buffer->export_data(out_buf_)};
                async_write({out_buf_.data(), static_cast<size_t>(len)},
                            [this](boost::system::error_code const& ec,
                                   size_t nbytes [[maybe_unused]]) mutable {
                                    if (ec) {
                                            spdlog::error("[WRITE FAIL] {}", ec.what());
                                            return;
                                    }
                                    out_queue_.pop();
                                    if (!out_queue_.empty()) send_front_pkt_out();
                            });
        }

        void process(raw_packet&& pkt) {
                out_queue_.push(std::move(pkt));
                if (1 == out_queue_.size()) send_front_pkt_out();
        }

        void async_receive() {
                async_read_some(in_buf_,
                                [this](boost::system::error_code const& ec, size_t nbytes) {
                                        if (ec) {
                                                spdlog::error("[RECEIVE FAIL] {}", ec.what());
                                                return;
                                        }
                                        spdlog::debug("[RECEIVE] {}", nbytes);
                                        net_.eth().receive(mstack::raw_packet{
                                                .buffer = std::make_unique<mstack::base_packet>(
                                                        std::span{in_buf_.data(), nbytes}),
                                        });
                                        async_receive();
                                });
        }

public:
        explicit tap(netns& net = netns::_default_(), std::string_view name = "")
            : net_(net), pfd_(net_.io_context_execution()) {
                auto fd{
                        file_desc::open("/dev/net/tun", file_desc::RDWR | file_desc::NONBLOCK),
                };

                if (!fd) {
                        spdlog::critical("[INIT FAIL]");
                        return;
                }

                // ? something error
                fd_ = std::move(fd.value());

                spdlog::debug("[DEV FD] {}", fd_.get_fd());

                ifreq ifr{};

                ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

                if (!name.empty()) std::ranges::copy(name, std::begin(ifr.ifr_name));

                if (int err{fd_.ioctl(TUNSETIFF, ifr)}; err < 0) {
                        spdlog::critical("[INIT FAIL]");
                        return;
                }

                std::copy(std::begin(ifr.ifr_name), std::end(ifr.ifr_name),
                          std::back_inserter(ndev_));

                if (utils::set_interface_up(ndev_) != 0) {
                        spdlog::critical("[SET UP] {}", ndev_);
                        return;
                }

                set_mac_addr();

                spdlog::debug("[INIT MAC] {}", mac_addr_);

                pfd_.assign(fd_.get_fd());

                net_.eth().under_handler_update(
                        std::bind(&tap::process, this, std::placeholders::_1));

                async_receive();
        }

        ~tap() = default;

        tap(const tap&) = delete;
        tap(tap&&)      = delete;

        tap& operator=(const tap&) = delete;
        tap& operator=(tap&& x)    = delete;

        std::string const& name() const { return ndev_; }

        auto& get_executor() { return pfd_.get_executor(); }

        std::optional<ipv4_addr_t> ipv4_addr() const { return ipv4_addr_; }

        void set_ipv4_addr(ipv4_addr_t const& ipv4_addr) {
                reset_ipv4_addr();
                ipv4_addr_ = ipv4_addr;
                net_.arp_cache().update({ipv4_addr, mac_addr_});
                net_.rt().update({ipv4_addr, ipv4_addr});
        }

        void be_srch_for(ipv4_addr_t const& ipv4_addr) {
                if (ipv4_addr_ != ipv4_addr) net_.rt().update({ipv4_addr, *ipv4_addr_});
        }

        void reset_ipv4_addr() {
                if (ipv4_addr_) {
                        net_.arp_cache().reset(*ipv4_addr_);
                        net_.rt().reset(*ipv4_addr_);
                        ipv4_addr_.reset();
                }
        }

        netns&       net() { return net_; }
        netns const& net() const { return net_; }

        mac_addr_t const& mac_addr() const { return mac_addr_; }
};

}  // namespace mstack
