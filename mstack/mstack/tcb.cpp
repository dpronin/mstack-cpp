#include "tcb.hpp"

#include <cassert>
#include <cstddef>

#include <algorithm>
#include <functional>
#include <memory>
#include <queue>
#include <vector>

#include "base_packet.hpp"
#include "circle_buffer.hpp"
#include "defination.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcb_manager.hpp"
#include "tcp_header.hpp"

namespace mstack {

tcb_t::tcb_t(boost::asio::io_context&                               io_ctx,
             tcb_manager&                                           mngr,
             std::shared_ptr<circle_buffer<std::shared_ptr<tcb_t>>> active_tcbs,
             std::shared_ptr<listener_t>                            listener,
             ipv4_port_t const&                                     remote_info,
             ipv4_port_t const&                                     local_info)
    : io_ctx_(io_ctx),
      mngr_(mngr),
      active_tcbs_(std::move(active_tcbs)),
      listener_(std::move(listener)),
      local_info_(local_info),
      remote_info_(remote_info),
      state_(TCP_CLOSED) {
        assert(active_tcbs_);
        assert(listener_);
}

void tcb_t::enqueue_send(std::span<std::byte const> packet) {
        send_queue_.push({packet.begin(), packet.end()});
        if (active_tcbs_->size() < send_queue_.size()) {
                active_tcbs_->push(shared_from_this());
                mngr_.activate();
        }
}

void tcb_t::listen_finish() {
        if (!listener_->on_acceptor_has_tcb.empty()) {
                auto cb{std::move(listener_->on_acceptor_has_tcb.front())};
                listener_->on_acceptor_has_tcb.pop();
                io_ctx_.post([this, cb = std::move(cb)] { cb(shared_from_this()); });
        } else {
                listener_->acceptors.push(shared_from_this());
        }
}

void tcb_t::active_self() {
        if (active_tcbs_->empty()) active_tcbs_->push(shared_from_this());
}

std::unique_ptr<base_packet> tcb_t::prepare_data_optional(int& option_len) { return {}; }

std::optional<tcp_packet_t> tcb_t::make_packet() {
        std::optional<tcp_packet_t> r;

        tcp_header_t                 out_tcp;
        std::unique_ptr<base_packet> out_buffer;

        int option_len{0};

        std::unique_ptr<base_packet> data_buffer{prepare_data_optional(option_len)};

        auto pkt{std::vector<std::byte>{}};

        if (!send_queue_.empty()) pkt = send_queue_.pop().value();

        if (data_buffer)
                out_buffer = std::move(data_buffer);
        else
                out_buffer = std::make_unique<base_packet>(tcp_header_t::size() + pkt.size());

        out_tcp.src_port = local_info_.port_addr;
        out_tcp.dst_port = remote_info_.port_addr;
        out_tcp.ack_no   = receive_.next;
        out_tcp.seq_no   = pkt.empty() ? send_.unacknowledged : send_.next;

        send_.next += pkt.size();

        // TODO
        out_tcp.window_size   = 0xFAF0;
        out_tcp.header_length = (tcp_header_t::size() + option_len) / 4;

        out_tcp.ACK = 1;
        out_tcp.PSH = static_cast<bool>(!pkt.empty());

        if (this->next_state_ == TCP_SYN_RECEIVED) out_tcp.SYN = 1;

        std::ranges::copy(pkt,
                          out_buffer->get_pointer() + out_tcp.produce(out_buffer->get_pointer()));

        tcp_packet_t out_packet{
                .proto       = 0x06,
                .remote_info = this->remote_info_,
                .local_info  = this->local_info_,
                .buffer      = std::move(out_buffer),
        };

        if (this->next_state_ != this->state_) this->state_ = this->next_state_;

        r = std::move(out_packet);

        return r;
}

std::optional<tcp_packet_t> tcb_t::gather_packet() {
        return ctl_packets_.empty() ? make_packet() : ctl_packets_.pop();
}

}  // namespace mstack
