#include "netns.hpp"

#include <memory>

#include "arp.hpp"
#include "arp_cache.hpp"
#include "device.hpp"
#include "ethernet.hpp"
#include "icmp.hpp"
#include "ipv4.hpp"
#include "routing_table.hpp"
#include "skbuff.hpp"
#include "tcb_manager.hpp"
#include "tcp.hpp"

namespace mstack {

class netns::impl {
public:
        explicit impl(boost::asio::io_context& io_ctx);
        ~impl() = default;

        impl(impl const&)            = delete;
        impl& operator=(impl const&) = delete;

        impl(impl&&)            = delete;
        impl& operator=(impl&&) = delete;

        ethernetv2&    eth() noexcept { return eth_; }
        arp_cache_t&   arp_cache() noexcept { return *arp_cache_; }
        routing_table& rt() noexcept { return *rt_; }
        ipv4&          ip() noexcept { return ipv4_; }
        class tcp&     tcp() noexcept { return tcp_; }
        tcb_manager&   tcb_m() noexcept { return tcb_m_; }

        boost::asio::io_context& io_context_execution() { return io_ctx_; }

private:
        boost::asio::io_context& io_ctx_;

        tcb_manager                    tcb_m_;
        class tcp                      tcp_;
        icmp                           icmp_;
        std::shared_ptr<arp_cache_t>   arp_cache_;
        arp                            arp_;
        std::shared_ptr<routing_table> rt_;
        ipv4                           ipv4_;
        ethernetv2                     eth_;
};

netns::impl::impl(boost::asio::io_context& io_ctx)
    : io_ctx_(io_ctx),
      tcb_m_(io_ctx_),
      tcp_(io_ctx_),
      icmp_(io_ctx_),
      arp_cache_(std::make_shared<arp_cache_t>()),
      arp_(io_ctx_, arp_cache_),
      rt_(std::make_shared<routing_table>()),
      ipv4_(io_ctx_, rt_, arp_),
      eth_(io_ctx_) {
        tcb_m_.under_proto_update(tcp_);
        tcp_.upper_proto_update(mstack::tcb_manager::PROTO, tcb_m_);
        tcp_.under_proto_update(ipv4_);
        icmp_.under_proto_update(ipv4_);
        ipv4_.upper_proto_update(mstack::icmp::PROTO, icmp_);
        ipv4_.upper_proto_update(mstack::tcp::PROTO, tcp_);
        ipv4_.under_proto_update(eth_);
        arp_.under_proto_update(eth_);
        eth_.upper_proto_update(mstack::ipv4::PROTO, ipv4_);
        eth_.upper_proto_update(mstack::arp::PROTO, arp_);
        eth_.under_handler_update([](skbuff&& skb_in, std::shared_ptr<device> dev) {
                dev->process(std::move(skb_in));
        });
}

netns::netns(boost::asio::io_context& io_ctx) : pimpl_(std::make_unique<impl>(io_ctx)) {}

netns::~netns() noexcept = default;

ethernetv2&    netns::eth() noexcept { return pimpl_->eth(); }
arp_cache_t&   netns::arp_cache() noexcept { return pimpl_->arp_cache(); }
routing_table& netns::rt() noexcept { return pimpl_->rt(); }
ipv4&          netns::ip() noexcept { return pimpl_->ip(); }
class tcp&     netns::tcp() noexcept { return pimpl_->tcp(); }
tcb_manager&   netns::tcb_m() noexcept { return pimpl_->tcb_m(); }

boost::asio::io_context& netns::io_context_execution() noexcept {
        return pimpl_->io_context_execution();
}

}  // namespace mstack
