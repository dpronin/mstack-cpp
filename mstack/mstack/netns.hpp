#pragma once

#include <memory>
#include <utility>

#include <boost/asio/io_context.hpp>

#include "mstack/arp_cache.hpp"
#include "mstack/ethernet.hpp"
#include "mstack/ipv4.hpp"
#include "mstack/routing_table.hpp"
#include "mstack/tcb_manager.hpp"

namespace mstack {

class netns {
public:
        static netns& _default_() {
                static netns net;
                return net;
        }

        template <typename... Args>
        static auto create(Args&&... args) {
                return std::unique_ptr<netns>{new netns{std::forward<Args>(args)...}};
        }

        ~netns() noexcept;

        netns(netns const&)            = delete;
        netns& operator=(netns const&) = delete;

        netns(netns&&)            = delete;
        netns& operator=(netns&&) = delete;

        ethernetv2&    eth() noexcept;
        arp_cache_t&   arp_cache() noexcept;
        routing_table& rt() noexcept;
        ipv4&          ip() noexcept;
        tcb_manager&   tcb_m() noexcept;

        boost::asio::io_context& io_context_execution() noexcept;

private:
        netns();

        class impl;
        std::unique_ptr<impl> pimpl_;
};

}  // namespace mstack
