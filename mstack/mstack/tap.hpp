#pragma once

#include <linux/if_tun.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>

#include <functional>
#include <optional>
#include <span>

#include "defination.hpp"
#include "file_desc.hpp"
#include "ipv4_addr.hpp"
#include "mac_addr.hpp"
#include "packets.hpp"
#include "utils.hpp"

namespace mstack {

template <int mtu>
class tap {
public:
        constexpr static int MTU = mtu;
        constexpr static int TAG = TUNTAP_DEV;

private:
        boost::asio::posix::stream_descriptor _pfd;
        file_desc                             _fd;
        std::optional<mac_addr_t>             _mac_addr;
        std::optional<ipv4_addr_t>            _ipv4_addr;
        std::string                           _ndev;

        bool _available = false;

        std::array<std::byte, MTU> _wbuf;
        std::array<std::byte, MTU> _rbuf;

        using packet_provider_type = std::function<std::optional<raw_packet>(void)>;
        using packet_receiver_type = std::function<void(raw_packet)>;

        packet_provider_type _provider_func;
        packet_receiver_type _receiver_func;

public:
        ~tap()                     = default;
        tap(const tap&)            = delete;
        tap(tap&&)                 = delete;
        tap& operator=(const tap&) = delete;
        tap& operator=(tap&& x)    = delete;

        static std::unique_ptr<tap> create(boost::asio::io_context& io_ctx) {
                return std::unique_ptr<tap>{new tap{io_ctx}};
        }

        operator bool() const { return _available; }

private:
        void set_mac_addr(std::string_view dev) {
                ifreq ifr{};

                strcpy(ifr.ifr_name, dev.data());

                if (int err{_fd.ioctl(SIOCGIFHWADDR, ifr)}; err < 0) SPDLOG_CRITICAL("[HW FAIL]");

                std::array<uint8_t, 6> hw_addr;
                for (int i = 0; i < 6; ++i)
                        hw_addr[i] = ifr.ifr_addr.sa_data[i];

                _mac_addr = mac_addr_t(hw_addr);
        }

        void async_read() {
                _pfd.async_read_some(
                        boost::asio::buffer(_rbuf), [this](auto const& ec, size_t nbytes) {
                                if (ec) return;

                                if (_receiver_func) {
                                        SPDLOG_DEBUG("[TAP RECEIVE] {}", nbytes);
                                        _receiver_func(encode_raw_packet({_rbuf.data(), nbytes}));
                                } else {
                                        SPDLOG_CRITICAL("[NO RECEIVER FUNC]");
                                }

                                async_read();
                        });
        }

        explicit tap(boost::asio::io_context& io_ctx) : _pfd(io_ctx) {
                auto fd{
                        file_desc::open("/dev/net/tun", file_desc::RDWR | file_desc::NONBLOCK),
                };

                if (!fd) {
                        SPDLOG_CRITICAL("[INIT FAIL]");
                        return;
                }

                // ? something error
                _fd = std::move(fd.value());

                SPDLOG_DEBUG("[DEV FD] {}", _fd.get_fd());

                ifreq ifr{};

                ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

                if (int err{_fd.ioctl(TUNSETIFF, ifr)}; err < 0) {
                        SPDLOG_CRITICAL("[INIT FAIL]");
                        return;
                }

                std::copy(std::begin(ifr.ifr_name), std::end(ifr.ifr_name),
                          std::back_inserter(_ndev));

                if (utils::set_interface_up(_ndev) != 0) {
                        SPDLOG_CRITICAL("[SET UP] {}", _ndev);
                        return;
                }

                set_mac_addr(_ndev);

                SPDLOG_DEBUG("[INIT MAC] {}", _mac_addr.value());

                _available = true;

                _pfd.assign(_fd.get_fd());

                async_read();
        }

        raw_packet encode_raw_packet(std::span<std::byte const> buf) {
                return {
                        .buffer = std::make_unique<base_packet>(buf),
                };
        }

        ssize_t decode_raw_packet(raw_packet& r_packet, std::span<std::byte> buf) {
                return r_packet.buffer->export_data(buf);
        }

        void async_write(raw_packet& pkt) {
                auto const len{decode_raw_packet(pkt, _wbuf)};
                SPDLOG_DEBUG("[TAP WRITE] {}", len);
                boost::asio::async_write(this->_pfd, boost::asio::buffer(_wbuf, len),
                                         [this](auto const& ec, size_t nbytes) {
                                                 if (!ec) notify_to_write();
                                         });
        }

        void notify_to_write() {
                if (auto pkt{_provider_func()}) {
                        async_write(*pkt);
                } else {
                        using namespace std::chrono_literals;
                        auto t{std::make_unique<boost::asio::steady_timer>(_pfd.get_executor())};
                        t->expires_after(100ms);
                        auto* pt{t.get()};
                        pt->async_wait([this, t = std::move(t)](auto const& ec) {
                                if (!ec) notify_to_write();
                        });
                }
        }

public:
        void capture(std::string_view route_pref) { utils::set_interface_route(_ndev, route_pref); }

        std::optional<mac_addr_t> get_mac_addr() const { return _mac_addr; }

        std::optional<ipv4_addr_t> get_ipv4_addr() const { return _ipv4_addr; }

        void set_ipv4_addr(ipv4_addr_t ipv4_addr) { _ipv4_addr = ipv4_addr; }

        template <typename Protocol>
        void register_upper_protocol(Protocol& protocol) {
                _provider_func = [&protocol] { return protocol.gather_packet(); };
                _receiver_func = [&protocol](auto r_packet) {
                        protocol.receive(std::move(r_packet));
                };
                notify_to_write();
        }
};

}  // namespace mstack
