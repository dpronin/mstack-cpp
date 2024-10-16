#pragma once

#include <cassert>
#include <cstddef>

#include <memory>
#include <queue>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include "netns.hpp"
#include "skbuff.hpp"

namespace mstack {

class netns;

class device : public std::enable_shared_from_this<device> {
private:
        netns&                                net_;
        boost::asio::posix::stream_descriptor pfd_;
        std::string                           ndev_;
        std::queue<skbuff>                    out_skb_q_;

        template <typename Completion>
        void async_read_some(std::span<std::byte> buf, Completion&& completion);

        template <typename Completion>
        void async_write(std::span<std::byte const> buf, Completion&& completion);

        void send_front_pkt_out();

        void async_receive();

        explicit device(netns& net = netns::_default_(), std::string_view name = "");

public:
        template <typename... Args>
        static std::shared_ptr<device> create(Args&&... args) {
                return std::shared_ptr<device>{new device{std::forward<Args>(args)...}};
        }
        ~device() noexcept;

        device(const device&) = delete;
        device(device&&)      = delete;

        device& operator=(const device&) = delete;
        device& operator=(device&&)      = delete;

        std::string const& name() const;

        auto& get_executor();

        void process(skbuff&& skb_in);

        netns&       net();
        netns const& net() const;
};

}  // namespace mstack
