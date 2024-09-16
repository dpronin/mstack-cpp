#include "netns.hpp"

#include <memory>

#include "arp.hpp"
#include "arp_cache.hpp"
#include "ethernet.hpp"
#include "icmp.hpp"
#include "ipv4.hpp"
#include "tcb_manager.hpp"
#include "tcp.hpp"

namespace mstack {

class netns::impl {
public:
        impl();
        ~impl() = default;

        impl(impl const&)            = delete;
        impl& operator=(impl const&) = delete;

        impl(impl&&)            = delete;
        impl& operator=(impl&&) = delete;

        ethernetv2&  eth() noexcept { return *eth_; }
        arp&         arpv4() noexcept { return *arp_; }
        ipv4&        ip() noexcept { return *ipv4_; }
        tcb_manager& tcb_m() noexcept { return *tcb_m_; }

        boost::asio::io_context& io_context_execution() { return io_ctx_; }

private:
        boost::asio::io_context io_ctx_;

        std::shared_ptr<tcb_manager> tcb_m_;
        std::shared_ptr<tcp>         tcp_;
        std::shared_ptr<icmp>        icmp_;
        std::shared_ptr<arp_cache_t> arp_cache_;
        std::shared_ptr<arp>         arp_;
        std::shared_ptr<ipv4>        ipv4_;
        std::shared_ptr<ethernetv2>  eth_;
};

netns::impl::impl()
    : tcb_m_(std::make_shared<tcb_manager>()),
      tcp_(std::make_shared<tcp>()),
      icmp_(std::make_shared<icmp>()),
      arp_cache_(std::make_shared<arp_cache_t>()),
      arp_(std::make_shared<arp>(arp_cache_)),
      ipv4_(std::make_shared<ipv4>(arp_cache_)),
      eth_(std::make_shared<ethernetv2>()) {
        tcp_->upper_handler_update(std::pair{mstack::tcb_manager::PROTO, tcb_m_});
        ipv4_->upper_handler_update(std::pair{mstack::icmp::PROTO, icmp_});
        ipv4_->upper_handler_update(std::pair{mstack::tcp::PROTO, tcp_});
        eth_->upper_handler_update(std::pair{mstack::ipv4::PROTO, ipv4_});
        eth_->upper_handler_update(std::pair{mstack::arp::PROTO, arp_});
}

netns::netns() : pimpl_(std::make_unique<impl>()) {}

netns::~netns() noexcept = default;

ethernetv2&  netns::eth() noexcept { return pimpl_->eth(); }
arp&         netns::arpv4() noexcept { return pimpl_->arpv4(); }
ipv4&        netns::ip() noexcept { return pimpl_->ip(); }
tcb_manager& netns::tcb_m() noexcept { return pimpl_->tcb_m(); }

boost::asio::io_context& netns::io_context_execution() noexcept {
        return pimpl_->io_context_execution();
}

}  // namespace mstack
