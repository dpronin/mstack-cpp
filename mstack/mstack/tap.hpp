#pragma once

#include <cassert>
#include <cstddef>

#include <memory>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include "file_desc.hpp"
#include "ipv4_addr.hpp"
#include "mac_addr.hpp"
#include "netns.hpp"
#include "raw_packet.hpp"

namespace mstack {

class netns;

class tap : public std::enable_shared_from_this<tap> {
private:
        netns&                                net_;
        boost::asio::posix::stream_descriptor pfd_;
        file_desc                             fd_;
        mac_addr_t                            mac_addr_;
        std::optional<ipv4_addr_t>            ipv4_addr_;
        std::string                           ndev_;
        std::queue<raw_packet>                out_queue_;

        template <typename Completion>
        void async_read_some(std::span<std::byte> buf, Completion&& completion);

        template <typename Completion>
        void async_write(std::span<std::byte const> buf, Completion&& completion);

        void send_front_pkt_out();

        void async_receive();

        explicit tap(netns& net = netns::_default_(), std::string_view name = "");

public:
        template <typename... Args>
        static std::shared_ptr<tap> create(Args&&... args) {
                return std::shared_ptr<tap>{new tap{std::forward<Args>(args)...}};
        }
        ~tap() = default;

        tap(const tap&) = delete;
        tap(tap&&)      = delete;

        tap& operator=(const tap&) = delete;
        tap& operator=(tap&&)      = delete;

        std::string const& name() const;

        auto& get_executor();

        void process(raw_packet&& pkt);

        void                       set_ipv4_addr(ipv4_addr_t const& ipv4_addr);
        std::optional<ipv4_addr_t> ipv4_addr() const;
        void                       reset_ipv4_addr();

        mac_addr_t const& mac_addr() const;

        netns&       net();
        netns const& net() const;
};

}  // namespace mstack
