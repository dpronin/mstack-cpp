#pragma once

#include <linux/if_tun.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/write.hpp>

#include <optional>
#include <span>

#include "file_desc.hpp"
#include "ipv4_addr.hpp"
#include "utils.hpp"

namespace mstack {

class tun {
private:
        boost::asio::posix::stream_descriptor _pfd;
        file_desc                             _fd;
        std::optional<ipv4_addr_t>            _ipv4_addr;
        std::string                           _ndev;

public:
        explicit tun(boost::asio::io_context& io_ctx) : _pfd(io_ctx) {
                auto fd{
                        file_desc::open("/dev/net/tun", file_desc::RDWR | file_desc::NONBLOCK),
                };

                if (!fd) {
                        spdlog::critical("[INIT FAIL]");
                        return;
                }

                _fd = std::move(fd.value());

                spdlog::debug("[DEV FD] {}", _fd.get_fd());

                ifreq ifr{};

                ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

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

                _pfd.assign(_fd.get_fd());
        }

        ~tun() = default;

        tun(const tun&) = delete;
        tun(tun&&)      = delete;

        tun& operator=(const tun&) = delete;
        tun& operator=(tun&& x)    = delete;

        template <typename Completion>
        void async_read_some(std::span<std::byte> buf, Completion&& completion) {
                spdlog::debug("[TUN READ SOME]: max bytes {}", buf.size());
                _pfd.async_read_some(boost::asio::buffer(buf),
                                     std::forward<Completion>(completion));
        }

        template <typename Completion>
        void async_write(std::span<std::byte const> buf, Completion&& completion) {
                spdlog::debug("[TUN WRITE]: exactly bytes {}", buf.size());
                boost::asio::async_write(this->_pfd, boost::asio::buffer(buf),
                                         std::forward<Completion>(completion));
        }

        std::string const& name() const { return _ndev; }

        auto& get_executor() { return _pfd.get_executor(); }

        void capture(std::string_view route_pref) { utils::set_interface_route(_ndev, route_pref); }

        std::optional<ipv4_addr_t> get_ipv4_addr() const { return _ipv4_addr; }

        void set_ipv4_addr(ipv4_addr_t ipv4_addr) { _ipv4_addr = ipv4_addr; }
};

}  // namespace mstack
