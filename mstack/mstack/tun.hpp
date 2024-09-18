#pragma once

#include <linux/if_tun.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/write.hpp>

#include <optional>
#include <span>
#include <string>

#include "api.hpp"
#include "file_desc.hpp"
#include "ipv4_addr.hpp"
#include "netns.hpp"
#include "utils.hpp"

namespace mstack {

class tun {
private:
        netns&                                net_;
        boost::asio::posix::stream_descriptor pfd_;
        file_desc                             fd_;
        std::optional<ipv4_addr_t>            ipv4_addr_;
        std::string                           ndev_;

public:
        explicit tun(netns& net) : net_(net), pfd_(net_.io_context_execution()) {
                auto fd{
                        file_desc::open("/dev/net/tun", file_desc::RDWR | file_desc::NONBLOCK),
                };

                if (!fd) {
                        spdlog::critical("[INIT FAIL]");
                        return;
                }

                fd_ = std::move(fd.value());

                spdlog::debug("[DEV FD] {}", fd_.get_fd());

                ifreq ifr{};

                ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

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

                pfd_.assign(fd_.get_fd());

                mstack::async_dev_read(
                        *this, std::make_unique_for_overwrite<std::array<std::byte, 1500>>(),
                        [this](std::span<std::byte const> buf) {
                                this->net().ip().receive(mstack::raw_packet{
                                        .buffer = std::make_unique<mstack::base_packet>(buf),
                                });
                        });

                mstack::async_dev_write_tick(
                        *this, std::make_unique_for_overwrite<std::array<std::byte, 1500>>(),
                        [this] { return this->net().ip().gather_packet(); });
        }

        tun() : tun(netns::_default_()) {}

        ~tun() = default;

        tun(const tun&) = delete;
        tun(tun&&)      = delete;

        tun& operator=(const tun&) = delete;
        tun& operator=(tun&& x)    = delete;

        template <typename Completion>
        void async_read_some(std::span<std::byte> buf, Completion&& completion) {
                spdlog::debug("[TUN READ SOME]: max bytes {}", buf.size());
                pfd_.async_read_some(boost::asio::buffer(buf),
                                     std::forward<Completion>(completion));
        }

        template <typename Completion>
        void async_write(std::span<std::byte const> buf, Completion&& completion) {
                spdlog::debug("[TUN WRITE]: exactly bytes {}", buf.size());
                boost::asio::async_write(pfd_, boost::asio::buffer(buf),
                                         std::forward<Completion>(completion));
        }

        std::string const& name() const { return ndev_; }

        std::optional<ipv4_addr_t> ipv4_addr() const { return ipv4_addr_; }

        void set_ipv4_addr(ipv4_addr_t ipv4_addr) { ipv4_addr_ = ipv4_addr; }

        netns&       net() { return net_; }
        netns const& net() const { return net_; }
};

}  // namespace mstack
