#pragma once

#include <linux/if_tun.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <optional>
#include <span>
#include <string>

#include "arp.hpp"
#include "file_desc.hpp"
#include "ipv4_addr.hpp"
#include "mac_addr.hpp"
#include "utils.hpp"

namespace mstack {

class tap {
private:
        boost::asio::posix::stream_descriptor _pfd;
        file_desc                             _fd;
        mac_addr_t                            _mac_addr;
        std::optional<ipv4_addr_t>            _ipv4_addr;
        std::string                           _ndev;

        void set_mac_addr() {
                ifreq ifr{};

                strcpy(ifr.ifr_name, _ndev.data());

                if (int err{_fd.ioctl(SIOCGIFHWADDR, ifr)}; err < 0) {
                        throw std::runtime_error(
                                std::format("failed to set MAC address, [HW FAIL], err {}", err));
                }

                std::array<std::byte, 6> hw_addr;
                for (int i = 0; i < 6; ++i)
                        hw_addr[i] = static_cast<std::byte>(ifr.ifr_addr.sa_data[i]);

                _mac_addr = mac_addr_t{hw_addr};
        }

public:
        explicit tap(boost::asio::io_context& io_ctx) : _pfd(io_ctx) {
                auto fd{
                        file_desc::open("/dev/net/tun", file_desc::RDWR | file_desc::NONBLOCK),
                };

                if (!fd) {
                        spdlog::critical("[INIT FAIL]");
                        return;
                }

                // ? something error
                _fd = std::move(fd.value());

                spdlog::debug("[DEV FD] {}", _fd.get_fd());

                ifreq ifr{};

                ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

                if (int err{_fd.ioctl(TUNSETIFF, ifr)}; err < 0) {
                        spdlog::critical("[INIT FAIL]");
                        return;
                }

                std::copy(std::begin(ifr.ifr_name), std::end(ifr.ifr_name),
                          std::back_inserter(_ndev));

                if (utils::set_interface_up(_ndev) != 0) {
                        spdlog::critical("[SET UP] {}", _ndev);
                        return;
                }

                set_mac_addr();

                spdlog::debug("[INIT MAC] {}", _mac_addr);

                _pfd.assign(_fd.get_fd());
        }

        ~tap() = default;

        tap(const tap&) = delete;
        tap(tap&&)      = delete;

        tap& operator=(const tap&) = delete;
        tap& operator=(tap&& x)    = delete;

        template <typename Completion>
        void async_read_some(std::span<std::byte> buf, Completion&& completion) {
                spdlog::debug("[TUN READ SOME]: max bytes {}", buf.size());
                _pfd.async_read_some(boost::asio::buffer(buf),
                                     std::forward<Completion>(completion));
        }

        template <typename Completion>
        void async_write(std::span<std::byte const> buf, Completion&& completion) {
                spdlog::debug("[TUN WRITE]: exactly bytes {}", buf.size());
                boost::asio::async_write(_pfd, boost::asio::buffer(buf),
                                         std::forward<Completion>(completion));
        }

        std::string const& name() const { return _ndev; }

        auto& get_executor() { return _pfd.get_executor(); }

        void capture(std::string_view route_pref) { utils::set_interface_route(_ndev, route_pref); }

        std::optional<ipv4_addr_t> ipv4_addr() const { return _ipv4_addr; }

        void set_ipv4_addr(ipv4_addr_t const& ipv4_addr) {
                reset_ipv4_addr();
                _ipv4_addr = ipv4_addr;
                arp::instance().add({ipv4_addr, _mac_addr});
        }

        void reset_ipv4_addr() {
                if (_ipv4_addr) {
                        arp::instance().remove(*_ipv4_addr);
                        _ipv4_addr.reset();
                }
        }

        mac_addr_t const& mac_addr() const { return _mac_addr; }
};

}  // namespace mstack
