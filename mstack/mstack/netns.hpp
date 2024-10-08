#pragma once

#include <memory>

#include <boost/asio/io_context.hpp>

#include "ethernet.hpp"
#include "ipv4.hpp"
#include "neigh_cache.hpp"
#include "routing_table.hpp"
#include "tcb_manager.hpp"
#include "tcp.hpp"

namespace mstack {

class netns {
public:
        static netns& _default_() {
                static boost::asio::io_context io_ctx;
                static netns                   net{io_ctx};
                return net;
        }

        explicit netns(boost::asio::io_context& io_ctx);
        explicit netns() : netns(_default_().io_context_execution()) {}

        ~netns() noexcept;

        netns(netns const&)            = delete;
        netns& operator=(netns const&) = delete;

        netns(netns&&)            = delete;
        netns& operator=(netns&&) = delete;

        ethernetv2&    eth() noexcept;
        neigh_cache&   neighs() noexcept;
        routing_table& rt() noexcept;
        ipv4&          ip() noexcept;
        class tcp&     tcp() noexcept;
        tcb_manager&   tcb_m() noexcept;

        boost::asio::io_context& io_context_execution() noexcept;

private:
        class impl;
        std::unique_ptr<impl> pimpl_;
};

}  // namespace mstack
